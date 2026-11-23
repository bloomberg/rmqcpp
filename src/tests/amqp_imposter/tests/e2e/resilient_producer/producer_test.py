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

from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass

import pytest

from amqp_imposter.control.client import (
    amqp_imposter,
    pb,
    seek_transcript_line,
    weighted,
    when_receive,
    when_send,
)

from ..generators import (
    GEN_BIT,
    GEN_MESSAGE_COUNT,
    GEN_NONEMPTY_NAME,
    GEN_SHORTSTR,
    client_randomness,
)
from ..property import assert_properties
from .producer import Message, ResilientProducer
from .producer_properties import producer_properties

SEEDS = range(100)
TIMEOUT = 10  # real clients often have backoff so they may not be this fast by default


@dataclass(frozen=True, slots=True)
class ProducerScenario:
    exchange: str
    routing_key: str
    messages: tuple[Message, ...]


def producer_scenario(seed: int) -> ProducerScenario:
    randomness = client_randomness(seed)
    message_count = GEN_MESSAGE_COUNT.from_randomness(randomness)

    return ProducerScenario(
        exchange=GEN_NONEMPTY_NAME.from_randomness(randomness),
        routing_key=GEN_SHORTSTR.from_randomness(randomness),
        messages=tuple(
            # some mandatory, some not
            Message(f"m{i}", mandatory=GEN_BIT.from_randomness(randomness))
            for i in range(message_count)
        ),
    )


@pytest.mark.timeout(TIMEOUT)
@pytest.mark.parametrize("seed", SEEDS)
def test_interleave_returns_teardown(seed: int) -> None:
    scenario = producer_scenario(seed)
    config = pb.SessionConfig(
        weights=(
            weighted(pb.BasicReturnSelector(), pb.BASE_WEIGHT // 4),
            weighted(pb.BasicAckSelector(), pb.BASE_WEIGHT // 2),
            weighted(
                pb.ConnectionCloseSelector(class_id=0, method_id=0),
                pb.BASE_WEIGHT // 20,
            ),
        ),
        seed=seed,
    )

    with amqp_imposter(config) as imposter:
        producer = ResilientProducer(
            amqp_port=imposter.amqp_port,
            exchange=scenario.exchange,
            routing_key=scenario.routing_key,
            messages=scenario.messages,
        )
        unroutable_message_ids = producer.publish_until_acked()

        message_ids = frozenset(m.message_id for m in scenario.messages)
        assert_properties(
            imposter.transcript(),
            producer_properties(message_ids, unroutable_message_ids),
        )


@pytest.mark.timeout(TIMEOUT)
@pytest.mark.parametrize("seed", SEEDS)
def test_return_single_ack_multiple(seed: int) -> None:
    scenario = producer_scenario(seed)

    tag_count = len(scenario.messages)
    config = pb.SessionConfig(
        script=(
            when_receive(
                pb.BasicPublishPattern(mandatory=True),
                then_send=pb.BasicReturnPattern(),
            ),
            when_receive(
                pb.BasicPublishPattern(mandatory=False),
                then_send=pb.BasicAckPattern(delivery_tag=tag_count, multiple=True),
            ),
            when_send(
                pb.BasicReturnPattern(),
                then_send=pb.BasicAckPattern(delivery_tag=tag_count, multiple=True),
            ),
        ),
        weights=(
            weighted(pb.BasicAckSelector(), 0),
            weighted(pb.BasicNackSelector(), 0),
        ),
        seed=seed,
    )

    with amqp_imposter(config) as imposter:
        producer = ResilientProducer(
            amqp_port=imposter.amqp_port,
            exchange=scenario.exchange,
            routing_key=scenario.routing_key,
            messages=scenario.messages,
        )
        unroutable_message_ids = producer.publish_until_acked()

        message_ids = frozenset(m.message_id for m in scenario.messages)
        assert unroutable_message_ids == {
            m.message_id for m in scenario.messages if m.mandatory
        }
        assert_properties(
            imposter.transcript(),
            producer_properties(message_ids, unroutable_message_ids),
        )


@pytest.mark.timeout(TIMEOUT)
@pytest.mark.parametrize("seed", SEEDS)
def test_reconfigure_while_unconfirmed(seed: int) -> None:
    scenario = producer_scenario(seed)
    config = pb.SessionConfig(
        seed=seed,
        weights=(
            weighted(pb.BasicAckSelector(), 0),
            weighted(pb.BasicNackSelector(), 0),
            weighted(pb.BasicReturnSelector(), 0),
        ),
    )

    last_message_id = scenario.messages[-1].message_id

    with amqp_imposter(config) as imposter, ThreadPoolExecutor() as tp:
        producer = ResilientProducer(
            amqp_port=imposter.amqp_port,
            exchange=scenario.exchange,
            routing_key=scenario.routing_key,
            messages=scenario.messages,
        )

        # watch transcript until the publishes are done
        watcher = tp.submit(
            seek_transcript_line,
            imposter.watch_transcript(timeout=TIMEOUT / 2),
            lambda line: (
                line.method.basic_publish.properties.message_id == last_message_id
            ),
        )
        # then drop TCP and reconfigure
        watcher.add_done_callback(
            lambda _: imposter.configure(pb.SessionConfig(seed=seed))
        )

        unroutable_message_ids = producer.publish_until_acked()
        assert watcher.result()

        message_ids = frozenset(m.message_id for m in scenario.messages)

        assert_properties(
            imposter.transcript(),
            producer_properties(message_ids, unroutable_message_ids),
        )
