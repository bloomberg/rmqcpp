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

from dataclasses import dataclass

import pytest
from pika.adapters.blocking_connection import BlockingChannel
from pika.exchange_type import ExchangeType

from amqp_imposter.control.client import (
    NOT_FORCED_REPLY_TEXT,
    AmqpImposter,
    amqp_imposter,
    pb,
    weighted,
    when_receive,
    when_send,
)

from ..generators import (
    GEN_DELIVERIES,
    GEN_NONEMPTY_NAME,
    GEN_NONEMPTY_SHORTSTR,
    GEN_PREFETCH_COUNT,
    GEN_SHORTSTR,
    client_randomness,
    distinct_names,
)
from ..property import Property, assert_properties
from .consumer import Delivery, ResilientConsumer
from .consumer_properties import Binding, Declaration, Exchange, Queue, properties

SEEDS = range(100)
TIMEOUT = 5  # real clients often have backoff so they may not be this fast by default

ANONYMOUS_QUEUE = ""


@dataclass(frozen=True, slots=True)
class ConsumerScenario:
    exchange: str
    routing_key: str
    dl_exchange: str
    dl_queue: str
    consumer_tag: str
    prefetch_count: int
    deliveries: int

    def topology(self) -> frozenset[Declaration]:
        return frozenset(
            (
                Exchange(self.exchange),
                Exchange(self.dl_exchange),
                Queue(self.dl_queue),
                Queue(ANONYMOUS_QUEUE),
                Binding(self.dl_exchange, ""),
                Binding(self.exchange, self.routing_key),
            )
        )

    def declare_topology(self, channel: BlockingChannel) -> str:
        channel.exchange_declare(self.exchange, ExchangeType.topic, durable=True)
        channel.exchange_declare(self.dl_exchange, ExchangeType.fanout, durable=True)
        channel.queue_declare(self.dl_queue, durable=True)
        channel.queue_bind(self.dl_queue, self.dl_exchange, routing_key="")

        queue: str = channel.queue_declare(
            ANONYMOUS_QUEUE,
            exclusive=True,
            arguments={"x-dead-letter-exchange": self.dl_exchange},
        ).method.queue

        channel.queue_bind(queue, self.exchange, routing_key=self.routing_key)

        return queue

    def properties(self) -> tuple[Property, ...]:
        return properties(self.prefetch_count, self.deliveries, self.topology())


def consumer_scenario(seed: int) -> ConsumerScenario:
    randomness = client_randomness(seed)
    exchange, dl_exchange = distinct_names(randomness, 2)

    return ConsumerScenario(
        exchange=exchange,
        routing_key=GEN_SHORTSTR.from_randomness(randomness),
        dl_exchange=dl_exchange,
        dl_queue=GEN_NONEMPTY_NAME.from_randomness(randomness),
        consumer_tag=GEN_NONEMPTY_SHORTSTR.from_randomness(randomness),
        prefetch_count=GEN_PREFETCH_COUNT.from_randomness(randomness),
        deliveries=GEN_DELIVERIES.from_randomness(randomness),
    )


def consume(imposter: AmqpImposter, scenario: ConsumerScenario) -> None:
    consumer = ResilientConsumer(
        amqp_port=imposter.amqp_port,
        declare_topology=scenario.declare_topology,
        consumer_tag=scenario.consumer_tag,
        prefetch_count=scenario.prefetch_count,
    )
    handled = 0

    def on_delivery(delivery: Delivery) -> None:
        nonlocal handled

        if handled % 2 == 0:
            delivery.ack()
        else:
            delivery.nack()

        handled += 1
        if handled >= scenario.deliveries:
            consumer.finish()

    consumer.start(on_delivery)


@pytest.mark.timeout(TIMEOUT)
@pytest.mark.parametrize("seed", SEEDS)
def test_stable_connection(seed: int) -> None:
    scenario = consumer_scenario(seed)
    config = pb.SessionConfig(seed=seed)

    with amqp_imposter(config) as imposter:
        consume(imposter, scenario)

        assert_properties(imposter.transcript(), scenario.properties())


@pytest.mark.timeout(TIMEOUT)
@pytest.mark.parametrize("seed", SEEDS)
def test_connection_close_and_cancel(seed: int) -> None:
    scenario = consumer_scenario(seed)
    config = pb.SessionConfig(
        seed=seed,
        weights=(
            weighted(
                pb.ConnectionClosePattern(class_id=0, method_id=0),
                pb.BASE_WEIGHT // 50,
            ),
            weighted(pb.BasicCancelPattern(), pb.BASE_WEIGHT // 10),
        ),
    )

    with amqp_imposter(config) as imposter:
        consume(imposter, scenario)

        assert_properties(imposter.transcript(), scenario.properties())


@pytest.mark.timeout(TIMEOUT)
@pytest.mark.parametrize("seed", SEEDS)
def test_channel_close_then_connection_close(seed: int) -> None:
    scenario = consumer_scenario(seed)
    config = pb.SessionConfig(
        script=(
            when_send(
                pb.BasicCancelPattern(),
                then_send=pb.ConnectionClosePattern(class_id=0, method_id=0),
            ),
            when_receive(
                pb.ChannelCloseOkPattern(),
                then_send=pb.ConnectionClosePattern(class_id=0, method_id=0),
            ),
        ),
        weights=(
            weighted(
                pb.ChannelClosePattern(reply_text=NOT_FORCED_REPLY_TEXT),
                pb.BASE_WEIGHT // 10,
            ),
            weighted(pb.BasicCancelPattern(), pb.BASE_WEIGHT // 10),
        ),
        seed=seed,
    )

    with amqp_imposter(config) as imposter:
        consume(imposter, scenario)

        assert_properties(imposter.transcript(), scenario.properties())


@pytest.mark.timeout(TIMEOUT)
@pytest.mark.parametrize("seed", SEEDS)
def test_channel_close_and_cancel(seed: int) -> None:
    scenario = consumer_scenario(seed)
    config = pb.SessionConfig(
        weights=(
            weighted(
                pb.ChannelClosePattern(reply_text=NOT_FORCED_REPLY_TEXT),
                pb.BASE_WEIGHT // 10,
            ),
            weighted(pb.BasicCancelPattern(), pb.BASE_WEIGHT // 50),
        ),
        seed=seed,
    )

    with amqp_imposter(config) as imposter:
        consume(imposter, scenario)

        assert_properties(imposter.transcript(), scenario.properties())
