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
)

from .generators import random_amqp_name
from .prop_consumer import (
    check_ack_count,
    check_consumer_recovers_within,
    check_consumer_stays_stopped,
    check_consumes,
    check_delivery_count,
    check_expected_requeue,
    check_qos_before_consuming,
)
from .prop_general import (
    Property,
    assert_properties,
    check_connections_closed,
    check_declarations_before_work,
    check_expected_declarations,
    check_open_backoff,
    check_protocol_legal,
)
from .runner import NEGOTIATION_OPTIONS, perftest_args, run_client

__all__ = [
    "test_consumer_with_teardowns",
    "test_consumer_without_teardowns",
]

SEEDS = range(500)


@dataclass(frozen=True, slots=True)
class ConsumerScenario:
    exchange: str
    queue: str
    routing_key: str
    messages: int
    prefetch_count: int
    priority: int
    auto_delete: bool


def consumer_scenario(seed: int) -> ConsumerScenario:
    rng = random.Random(seed)
    return ConsumerScenario(
        exchange=random_amqp_name(rng),
        queue=random_amqp_name(rng),
        routing_key=random_amqp_name(rng),
        messages=rng.randint(1, 30),
        prefetch_count=rng.randint(1, 30),
        priority=rng.randint(-10, 10),
        auto_delete=rng.random() < 0.5,
    )


def consumer_args(consumer: ConsumerScenario) -> tuple[str, ...]:
    return perftest_args(
        {
            "--exchange": consumer.exchange,
            "--type": "direct",
            "--queue": consumer.queue,
            "--routing-key": consumer.routing_key,
            "--consumers": "1",
            "--producers": "0",
            "--cmessages": str(consumer.messages),
            "--qos": str(consumer.prefetch_count),
            "--consumer-args": f"x-priority={consumer.priority}",
            "--auto-delete": str(consumer.auto_delete).lower(),
            **({} if consumer.auto_delete else {"--flag": "persistent"}),
            # TODO supporting --consumer-latency and varying by seed would help get to more bugs
        }
    )


def consumer_properties(
    consumer: ConsumerScenario, *, minimum_acks: int
) -> tuple[Property, ...]:
    exchange_declare = pb.ExchangeDeclare(
        exchange=consumer.exchange, exchange_type="direct", durable=True
    )
    queue_declare = pb.QueueDeclare(
        queue=consumer.queue,
        durable=not consumer.auto_delete,
        auto_delete=consumer.auto_delete,
    )
    queue_bind = pb.QueueBind(
        queue=consumer.queue,
        exchange=consumer.exchange,
        routing_key=consumer.routing_key,
    )
    return (
        check_protocol_legal,
        check_open_backoff,
        check_connections_closed,
        partial(check_qos_before_consuming, prefetch_count=consumer.prefetch_count),
        partial(check_consumes, queue=consumer.queue, priority=consumer.priority),
        partial(check_delivery_count, messages=consumer.messages),
        partial(check_ack_count, minimum=minimum_acks, maximum=consumer.messages),
        partial(check_consumer_stays_stopped, messages=consumer.messages),
        partial(check_consumer_recovers_within, messages=consumer.messages),
        partial(
            check_expected_requeue,
            requeue=True,  # TODO could try to vary this via perftest
        ),
        partial(
            check_declarations_before_work,
            required=(exchange_declare, queue_declare, queue_bind),
        ),
        partial(
            check_expected_declarations, expected=(exchange_declare, queue_declare)
        ),
    )


@pytest.mark.parametrize("seed", SEEDS)
def test_consumer_without_teardowns(seed: int, imposter: AmqpImposter) -> None:
    # GIVEN
    consumer = consumer_scenario(seed)
    imposter.configure(
        pb.SessionConfig(
            seed=seed,
            negotiation_options=NEGOTIATION_OPTIONS,
        )
    )

    # WHEN
    run = run_client(imposter, consumer_args(consumer))

    # THEN
    assert_properties(
        run, consumer_properties(consumer, minimum_acks=consumer.messages)
    )


@pytest.mark.parametrize("seed", SEEDS)
def test_consumer_with_teardowns(seed: int, imposter: AmqpImposter) -> None:
    # GIVEN
    consumer = consumer_scenario(seed)
    weights = (
        weighted(
            pb.ConnectionClosePattern(reply_text=NOT_FORCED_REPLY_TEXT),
            pb.BASE_WEIGHT // 20,
        ),
        weighted(
            pb.ChannelClosePattern(reply_text=NOT_FORCED_REPLY_TEXT),
            pb.BASE_WEIGHT // 20,
        ),
        weighted(pb.BasicCancelPattern(), pb.BASE_WEIGHT // 20),
    )
    # clients with backoff have long idle periods, so let them recover
    idle_weights = (
        weighted(pb.ConnectionClosePattern(class_id=0, method_id=0), 0),
        weighted(pb.BasicCancelPattern(), 0),
    )

    imposter.configure(
        pb.SessionConfig(
            seed=seed,
            negotiation_options=NEGOTIATION_OPTIONS,
            weights=weights,
            idle_weights=idle_weights,
        )
    )

    # WHEN
    run = run_client(imposter, consumer_args(consumer))

    # THEN
    assert_properties(run, consumer_properties(consumer, minimum_acks=0))
