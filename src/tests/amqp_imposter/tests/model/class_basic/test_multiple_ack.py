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

import pytest

from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    channel_errors,
    consuming,
    deliver,
    error_replies_to,
    lookup_sendables,
    match_fresh_delivery,
    message_content,
    prefetch_count_filled,
    receive_unwrap,
    subsets_any_sendable,
)

PREFETCH_COUNT = 3


def test_consume_multiple_ack() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicAck(delivery_tag=2, multiple=True))

    deliver(server, 4)
    deliver(server, 5)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicAck(delivery_tag=5, multiple=True))

    assert match_fresh_delivery(server).delivery_tag == 6


def test_multiple_ack_subrange() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    receive_unwrap(server, m.BasicAck(delivery_tag=1, multiple=True))

    deliver(server, 4)

    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_multiple_ack_does_not_requeue() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    receive_unwrap(server, m.BasicAck(delivery_tag=3, multiple=True))

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


def test_zero_multiple_ack_acknowledges_everything() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    receive_unwrap(server, m.BasicAck(delivery_tag=0, multiple=True))

    for delivery_tag in range(PREFETCH_COUNT + 1, 2 * PREFETCH_COUNT + 1):
        deliver(server, delivery_tag)

    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_zero_multiple_ack_does_not_requeue() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    receive_unwrap(server, m.BasicAck(delivery_tag=0, multiple=True))

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


def test_zero_multiple_ack_without_deliveries() -> None:
    server = consuming(prefetch_count=PREFETCH_COUNT)

    receive_unwrap(server, m.BasicAck(delivery_tag=0, multiple=True))

    deliver(server, 1)


def test_zero_multiple_nack_requeues_everything() -> None:
    server = consuming(prefetch_count=PREFETCH_COUNT)

    delivered = tuple(deliver(server, delivery_tag) for delivery_tag in (1, 2, 3))

    receive_unwrap(server, m.BasicNack(delivery_tag=0, multiple=True, requeue=True))

    redeliveries = lookup_sendables(server, m.BasicDeliver, redelivered=True)

    assert {message_content(d) for d in redeliveries} == {
        message_content(d) for d in delivered
    }


@pytest.mark.parametrize(
    "acknowledge",
    (m.BasicAck, m.BasicNack, m.BasicReject),
    ids=lambda k: k.__name__,
)
def test_zero_delivery_tag_alone_closes_channel(
    acknowledge: type[m.BasicAck | m.BasicNack | m.BasicReject],
) -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    receive_unwrap(server, acknowledge(0))

    assert error_replies_to(server, acknowledge) == channel_errors(
        mf.ReplyCode.PRECONDITION_FAILED
    )


def test_multiple_nack_requeue() -> None:
    server = consuming(prefetch_count=PREFETCH_COUNT)

    first = deliver(server, 1)
    second = deliver(server, 2)
    deliver(server, 3)

    receive_unwrap(server, m.BasicNack(delivery_tag=2, multiple=True, requeue=True))

    redeliveries = lookup_sendables(server, m.BasicDeliver, redelivered=True)

    assert {message_content(d) for d in redeliveries} == {
        message_content(first),
        message_content(second),
    }


def test_multiple_nack_no_requeue() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    receive_unwrap(server, m.BasicNack(delivery_tag=2, multiple=True, requeue=False))

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


def test_multiple_nack_destroys_deliveries() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    receive_unwrap(server, m.BasicNack(delivery_tag=2, multiple=True, requeue=False))

    deliver(server, 4)
    deliver(server, 5)

    assert not subsets_any_sendable(server, m.BasicDeliver)
