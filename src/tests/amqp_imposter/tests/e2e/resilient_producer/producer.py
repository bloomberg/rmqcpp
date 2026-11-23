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

from collections.abc import Iterable
from dataclasses import dataclass, field
from functools import partial

import pika
from pika.adapters.select_connection import SelectConnection
from pika.channel import Channel
from pika.connection import Connection
from pika.frame import Method
from pika.spec import Basic, BasicProperties

HOST = "127.0.0.1"


@dataclass(frozen=True, slots=True)
class Message:
    message_id: str
    mandatory: bool


def close_connection(connection: Connection) -> None:
    if connection.is_open:
        connection.close()


@dataclass(slots=True)
class ChannelPublisher:
    channel: Channel
    exchange: str
    routing_key: str
    message_ids_by_delivery_tag: dict[int, str] = field(default_factory=dict)
    returned_message_ids: set[str] = field(default_factory=set)
    next_delivery_tag: int = 1

    def publish(self, messages: Iterable[Message]) -> None:
        for message in messages:
            self.channel.basic_publish(
                self.exchange,
                self.routing_key,
                message.message_id.encode(),
                BasicProperties(message_id=message.message_id),
                mandatory=message.mandatory,
            )
            self.message_ids_by_delivery_tag[self.next_delivery_tag] = (
                message.message_id
            )
            self.returned_message_ids.discard(message.message_id)
            self.next_delivery_tag += 1

    def pop_confirmed(self, confirm: Basic.Ack | Basic.Nack) -> tuple[str, ...]:
        delivery_tags = (
            tuple(
                delivery_tag
                for delivery_tag in self.message_ids_by_delivery_tag
                if delivery_tag <= confirm.delivery_tag
            )
            if confirm.multiple
            else (confirm.delivery_tag,)
        )
        return tuple(
            self.message_ids_by_delivery_tag.pop(delivery_tag)
            for delivery_tag in delivery_tags
        )

    def on_return(
        self,
        channel: Channel,
        method: Basic.Return,
        properties: BasicProperties,
        body: bytes,
    ) -> None:
        if properties.message_id is not None:
            self.returned_message_ids.add(properties.message_id)


@dataclass(slots=True)
class ResilientProducer:
    amqp_port: int
    exchange: str
    routing_key: str
    messages: tuple[Message, ...]
    acked_message_ids: set[str] = field(default_factory=set, init=False)
    unroutable_message_ids: set[str] = field(default_factory=set, init=False)

    def outstanding_messages(self) -> tuple[Message, ...]:
        return tuple(
            m for m in self.messages if m.message_id not in self.acked_message_ids
        )

    def publish_until_acked(self) -> frozenset[str]:
        while self.outstanding_messages():
            self.hold_connection()

        return frozenset(self.unroutable_message_ids)

    def hold_connection(self) -> None:
        connection = SelectConnection(
            pika.ConnectionParameters(host=HOST, port=self.amqp_port),
            on_open_callback=lambda connection: connection.channel(
                on_open_callback=self.hold_channel
            ),
            on_open_error_callback=lambda connection, _: connection.ioloop.stop(),
            on_close_callback=lambda connection, _: connection.ioloop.stop(),
        )
        connection.ioloop.start()

    def hold_channel(self, channel: Channel) -> None:
        publisher = ChannelPublisher(channel, self.exchange, self.routing_key)

        channel.add_on_close_callback(
            lambda channel, _: close_connection(channel.connection)
        )
        channel.add_on_return_callback(publisher.on_return)
        channel.confirm_delivery(
            partial(self.on_confirm, publisher),
            callback=lambda _: publisher.publish(self.outstanding_messages()),
        )

    def on_confirm(
        self, publisher: ChannelPublisher, frame: Method[Basic.Ack | Basic.Nack]
    ) -> None:
        confirm = frame.method
        message_ids = publisher.pop_confirmed(confirm)

        match confirm:
            case Basic.Ack():
                self.acked_message_ids.update(message_ids)
                self.unroutable_message_ids.update(
                    publisher.returned_message_ids.intersection(message_ids)
                )
            case Basic.Nack():
                publisher.publish(
                    m for m in self.messages if m.message_id in message_ids
                )

        if not self.outstanding_messages():
            close_connection(publisher.channel.connection)
