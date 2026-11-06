# Copyright 2026 Bloomberg Finance L.P.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from collections.abc import Callable
from contextlib import suppress
from dataclasses import dataclass, field
from typing import Any

import pika
from pika.adapters.blocking_connection import BlockingChannel, BlockingConnection
from pika.exceptions import AMQPError, ChannelClosedByBroker, ConsumerCancelled
from pika.frame import Method
from pika.spec import Basic

HOST = "127.0.0.1"


@dataclass(frozen=True, slots=True)
class Delivery:
    channel: BlockingChannel
    delivery_tag: int

    def ack(self, multiple: bool = False) -> None:
        self.channel.basic_ack(self.delivery_tag, multiple=multiple)

    def nack(self, requeue: bool = True, multiple: bool = False) -> None:
        self.channel.basic_nack(self.delivery_tag, multiple=multiple, requeue=requeue)


type DeliveryCb = Callable[[Delivery], None]
type DeclareTopologyCb = Callable[[BlockingChannel], str]


def raise_consumer_cancelled(method_frame: Method[Basic.Cancel]) -> None:
    raise ConsumerCancelled(method_frame)


@dataclass(slots=True)
class ResilientConsumer:
    amqp_port: int
    declare_topology: DeclareTopologyCb
    consumer_tag: str
    prefetch_count: int = 0
    finished: bool = field(default=False, init=False)

    def finish(self) -> None:
        self.finished = True

    def start(self, on_delivery: DeliveryCb) -> None:
        while not self.finished:
            with suppress(AMQPError):
                self.hold_connection(on_delivery)

    def hold_connection(self, on_delivery: DeliveryCb) -> None:
        connection_params = pika.ConnectionParameters(host=HOST, port=self.amqp_port)
        connection = pika.BlockingConnection(connection_params)
        try:
            while not self.finished:
                self.hold_channel(connection, on_delivery)
        finally:
            with suppress(AMQPError):
                if connection.is_open:
                    connection.close(200, "finished")

    def hold_channel(
        self, connection: BlockingConnection, on_delivery: DeliveryCb
    ) -> None:
        channel = connection.channel()
        channel.add_on_cancel_callback(raise_consumer_cancelled)

        with suppress(ChannelClosedByBroker):
            channel.basic_qos(prefetch_count=self.prefetch_count)

            while not self.finished:
                with suppress(ConsumerCancelled):
                    queue = self.declare_topology(channel)
                    self.hold_consume(channel, queue, on_delivery)

    def hold_consume(
        self, channel: BlockingChannel, queue: str, on_delivery: DeliveryCb
    ) -> None:
        def on_message_callback(
            channel: BlockingChannel, method: Basic.Deliver, *_: Any
        ) -> None:
            if not self.finished:
                # stop processing so we're not stuck in callbacks forever
                on_delivery(Delivery(channel, method.delivery_tag))

        channel.basic_consume(
            queue,
            on_message_callback=on_message_callback,
            auto_ack=False,
            consumer_tag=self.consumer_tag,
        )

        while not self.finished:
            channel.connection.process_data_events(time_limit=0.05)
