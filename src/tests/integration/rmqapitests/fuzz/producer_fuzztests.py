"""Copyright 2026 Bloomberg Finance L.P.
SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
from __future__ import annotations

import random
from dataclasses import dataclass
from functools import partial

import pytest
from amqp_imposter.control.client import (
    NOT_FORCED_REPLY_TEXT,
    AmqpImposter,
    pb,
    weighted,
    when_receive,
)

from .generators import random_amqp_name
from .prop_general import (
    Property,
    assert_properties,
    check_connections_closed,
    check_declarations_before_work,
    check_expected_declarations,
    check_open_backoff,
    check_protocol_legal,
)
from .prop_producer import (
    check_confirm_select_before_publishing,
    check_every_publish_settled,
    check_expected_publishes,
    check_max_unconfirmed,
    check_message_count,
    check_producer_recovers_within,
    check_producer_stays_stopped,
)
from .runner import NEGOTIATION_OPTIONS, perftest_args, run_client

__all__ = [
    "test_producer_with_returns",
    "test_producer_with_teardowns",
    "test_producer_without_teardowns",
]

SEEDS = range(500)


@dataclass(frozen=True, slots=True)
class ProducerScenario:
    exchange: str
    queue: str
    routing_key: str
    messages: int
    confirm: int
    body_size: int
    auto_delete: bool


def producer_scenario(seed: int) -> ProducerScenario:
    rng = random.Random(seed)
    return ProducerScenario(
        exchange=random_amqp_name(rng),
        queue=random_amqp_name(rng),
        routing_key=random_amqp_name(rng),
        messages=rng.randint(1, 30),
        confirm=rng.randint(1, 30),
        body_size=rng.randint(1, 4096),
        auto_delete=rng.random() < 0.5,
    )


def producer_args(producer: ProducerScenario) -> tuple[str, ...]:
    return perftest_args(
        {
            "--exchange": producer.exchange,
            "--type": "direct",
            "--queue": producer.queue,
            "--routing-key": producer.routing_key,
            "--consumers": "0",
            "--producers": "1",
            "--pmessages": str(producer.messages),
            "--confirm": str(producer.confirm),
            "--size": str(producer.body_size),
            "--auto-delete": str(producer.auto_delete).lower(),
            **({} if producer.auto_delete else {"--flag": "persistent"}),
            # TODO support --message-properties
        }
    )


def producer_properties(producer: ProducerScenario) -> tuple[Property, ...]:
    exchange_declare = pb.ExchangeDeclare(
        exchange=producer.exchange, exchange_type="direct", durable=True
    )
    queue_declare = pb.QueueDeclare(
        queue=producer.queue,
        durable=not producer.auto_delete,
        auto_delete=producer.auto_delete,
    )
    return (
        check_protocol_legal,
        check_open_backoff,
        check_connections_closed,
        check_confirm_select_before_publishing,
        check_every_publish_settled,
        partial(check_max_unconfirmed, max_unconfirmed=producer.confirm),
        partial(check_message_count, messages=producer.messages),
        partial(check_producer_stays_stopped, messages=producer.messages),
        partial(check_producer_recovers_within, messages=producer.messages),
        partial(check_declarations_before_work, required=(exchange_declare,)),
        partial(
            check_expected_declarations, expected=(exchange_declare, queue_declare)
        ),
        partial(
            check_expected_publishes,
            exchange=producer.exchange,
            routing_key=producer.routing_key,
            persistent=not producer.auto_delete,
            body_size=producer.body_size,
        ),
    )


@pytest.mark.parametrize("seed", SEEDS)
def test_producer_without_teardowns(seed: int, imposter: AmqpImposter) -> None:
    # GIVEN
    producer = producer_scenario(seed)
    imposter.configure(
        pb.SessionConfig(
            seed=seed,
            negotiation_options=NEGOTIATION_OPTIONS,
        )
    )

    # WHEN
    run = run_client(imposter, producer_args(producer))

    # THEN
    assert_properties(run, producer_properties(producer))


@pytest.mark.parametrize("seed", SEEDS)
def test_producer_with_teardowns(seed: int, imposter: AmqpImposter) -> None:
    # GIVEN
    producer = producer_scenario(seed)
    weights = (
        weighted(
            pb.ConnectionClosePattern(reply_text=NOT_FORCED_REPLY_TEXT),
            pb.BASE_WEIGHT // 20,
        ),
        weighted(
            pb.ChannelClosePattern(reply_text=NOT_FORCED_REPLY_TEXT),
            pb.BASE_WEIGHT // 20,
        ),
    )

    # clients with backoff have long idle periods, so let them recover
    idle_weights = (weighted(pb.ConnectionClosePattern(class_id=0, method_id=0), 0),)

    imposter.configure(
        pb.SessionConfig(
            seed=seed,
            negotiation_options=NEGOTIATION_OPTIONS,
            weights=weights,
            idle_weights=idle_weights,
        )
    )

    # WHEN
    run = run_client(imposter, producer_args(producer))

    # THEN
    assert_properties(run, producer_properties(producer))


@pytest.mark.parametrize("seed", SEEDS)
def test_producer_with_returns(seed: int, imposter: AmqpImposter) -> None:
    # GIVEN
    producer = producer_scenario(seed)
    script = (
        when_receive(pb.BasicPublishPattern(), then_send=pb.BasicReturnPattern()),
    )
    weights = (
        weighted(pb.BasicAckPattern(), pb.BASE_WEIGHT // 2),
        weighted(
            pb.ConnectionClosePattern(class_id=0, method_id=0),
            pb.BASE_WEIGHT // 20,
        ),
    )
    idle_weights = (weighted(pb.ConnectionClosePattern(class_id=0, method_id=0), 0),)

    imposter.configure(
        pb.SessionConfig(
            seed=seed,
            negotiation_options=NEGOTIATION_OPTIONS,
            script=script,
            weights=weights,
            idle_weights=idle_weights,
        )
    )

    # WHEN
    run = run_client(imposter, producer_args(producer))

    # THEN
    assert_properties(run, producer_properties(producer))
