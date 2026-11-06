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

from amqp_imposter.model import Gen
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    ACKNOWLEDGEMENT_TYPES,
    channel_errors,
    consuming,
    error_replies_to,
    lookup_sendables,
    lookup_unique_sendable,
    match_fresh_delivery,
    message_content,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)


def test_consumer_reject_nack() -> None:
    server = consuming(prefetch_count=1)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=False))

    assert lookup_unique_sendable(server, m.BasicDeliver).delivery_tag == 2

    send_unwrap(server, m.BasicDeliver, delivery_tag=2)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicNack(delivery_tag=2, requeue=False))

    assert lookup_unique_sendable(server, m.BasicDeliver).delivery_tag == 3


def test_requeued_message_comes_back() -> None:
    server = consuming(prefetch_count=1)

    first = send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))

    again = lookup_unique_sendable(server, m.BasicDeliver, redelivered=True)

    assert again.delivery_tag == 2
    assert message_content(again) == message_content(first)


def test_fresh_delivery_and_requeue_delivery() -> None:
    # after Reject with requeue=True we should have:
    # - one fresh delivery with an arbitrary body and arbitrary requeued bit
    # - one requeued delivery with a concrete body and a concrete requeued bit
    server = consuming(prefetch_count=1)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))

    fresh = match_fresh_delivery(server)

    assert len(lookup_sendables(server, m.BasicDeliver)) == 2
    assert fresh.delivery_tag == 2
    assert all(isinstance(field, Gen) for field in message_content(fresh))


def test_reject_no_requeue() -> None:
    server = consuming(prefetch_count=1)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=False))

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


def test_accumulate_requeues() -> None:
    server = consuming(prefetch_count=2)

    first = send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    second = send_unwrap(server, m.BasicDeliver, delivery_tag=2)
    receive_unwrap(server, m.BasicNack(delivery_tag=1, requeue=True))
    receive_unwrap(server, m.BasicNack(delivery_tag=2, requeue=True))

    redeliveries = lookup_sendables(server, m.BasicDeliver, redelivered=True)

    assert {message_content(d) for d in redeliveries} == {
        message_content(first),
        message_content(second),
    }
    assert {d.delivery_tag for d in redeliveries} == {3}


def test_redelivery_persists() -> None:
    server = consuming(prefetch_count=1)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))
    send_unwrap(server, m.BasicDeliver, delivery_tag=2, redelivered=True)
    receive_unwrap(server, m.BasicReject(delivery_tag=2, requeue=True))

    assert subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


def test_tag_not_clobbered_after_reject() -> None:
    server = consuming(prefetch_count=1)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, m.BasicReject(delivery_tag=1))

    assert not subsets_any_sendable(server, m.BasicDeliver, delivery_tag=1)


@pytest.mark.parametrize("acknowledge", ACKNOWLEDGEMENT_TYPES, ids=lambda k: k.__name__)
def test_unknown_tag_closes_channel(
    acknowledge: type[m.BasicAck | m.BasicNack | m.BasicReject],
) -> None:
    server = consuming(prefetch_count=1)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, acknowledge(99))

    assert error_replies_to(server, acknowledge) == channel_errors(
        mf.ReplyCode.PRECONDITION_FAILED
    )


@pytest.mark.parametrize("acknowledge", ACKNOWLEDGEMENT_TYPES, ids=lambda k: k.__name__)
def test_repeat_acknowledgement_closes_channel(
    acknowledge: type[m.BasicAck | m.BasicNack | m.BasicReject],
) -> None:
    server = consuming(prefetch_count=1)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, acknowledge(1))
    receive_unwrap(server, acknowledge(1))

    assert error_replies_to(server, acknowledge) == channel_errors(
        mf.ReplyCode.PRECONDITION_FAILED
    )


@pytest.mark.parametrize("acknowledge", ACKNOWLEDGEMENT_TYPES, ids=lambda k: k.__name__)
def test_undelivered_tag_closes_channel(
    acknowledge: type[m.BasicAck | m.BasicNack | m.BasicReject],
) -> None:
    server = consuming(prefetch_count=1)

    receive_unwrap(server, acknowledge(1))

    assert error_replies_to(server, acknowledge) == channel_errors(
        mf.ReplyCode.PRECONDITION_FAILED
    )


@pytest.mark.parametrize(
    "acknowledge", (m.BasicAck, m.BasicNack), ids=lambda k: k.__name__
)
def test_multiple_tag_too_high_closes_channel(
    acknowledge: type[m.BasicAck | m.BasicNack],
) -> None:
    server = consuming(prefetch_count=3)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, acknowledge(delivery_tag=5, multiple=True))

    assert error_replies_to(server, acknowledge) == channel_errors(
        mf.ReplyCode.PRECONDITION_FAILED
    )
