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
    ACKNOWLEDGEMENT_TYPES,
    CONSUMER_TAG,
    DO_CANCEL,
    DO_CHANNEL_CLOSE,
    DO_CONNECTION_CLOSE,
    QUEUE,
    DoTeardown,
    channel_errors,
    channelled,
    consume,
    consuming_no_ack,
    deliver,
    error_replies_to,
    get,
    lookup_unique_sendable,
    receive_unwrap,
    reconnect,
    reopen_channel,
    send_get_ok,
    send_unwrap,
    subsets_any_sendable,
)


def test_no_ack_ignores_prefetch() -> None:
    server = consuming_no_ack()

    for delivery_tag in range(1, 100 + 1):
        deliver(server, delivery_tag)

    assert subsets_any_sendable(server, m.BasicDeliver)


@pytest.mark.parametrize("acknowledge", ACKNOWLEDGEMENT_TYPES, ids=lambda k: k.__name__)
def test_no_ack_acknowledgement_closes_channel(
    acknowledge: type[m.BasicAck | m.BasicNack | m.BasicReject],
) -> None:
    server = consuming_no_ack()

    deliver(server, 1)
    receive_unwrap(server, acknowledge(1))

    assert error_replies_to(server, acknowledge) == channel_errors(
        mf.ReplyCode.PRECONDITION_FAILED
    )


def test_no_ack_never_redelivers() -> None:
    server = consuming_no_ack()

    deliver(server, 1)

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


@pytest.mark.parametrize("cancel", DO_CANCEL, ids=lambda c: c.__name__)
def test_no_ack_cancel_requeues_nothing(cancel: DoTeardown) -> None:
    server = consuming_no_ack()

    deliver(server, 1)
    deliver(server, 2)
    cancel(server)
    consume(server)

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


@pytest.mark.parametrize("close", DO_CHANNEL_CLOSE, ids=lambda c: c.__name__)
def test_no_ack_channel_close_requeues_nothing(close: DoTeardown) -> None:
    server = consuming_no_ack()

    deliver(server, 1)
    deliver(server, 2)
    close(server)
    reopen_channel(server)
    consume(server)

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


@pytest.mark.parametrize("close", DO_CONNECTION_CLOSE, ids=lambda c: c.__name__)
def test_no_ack_connection_close_requeues_nothing(close: DoTeardown) -> None:
    server = consuming_no_ack()

    deliver(server, 1)
    deliver(server, 2)
    close(server)
    reconnect(server)
    consume(server)

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


def test_no_ack_get_ok_ack_closes_channel() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicGet(queue=QUEUE, no_ack=True))
    send_unwrap(server, m.BasicGetOk, delivery_tag=1)
    receive_unwrap(server, m.BasicAck(delivery_tag=1))

    assert error_replies_to(server, m.BasicAck) == channel_errors(
        mf.ReplyCode.PRECONDITION_FAILED
    )


def test_no_ack_consumer_leaves_get_acknowledgeable() -> None:
    server = consuming_no_ack()

    deliver(server, 1)
    receive_unwrap(server, m.BasicGet(queue=QUEUE))

    got = lookup_unique_sendable(server, m.BasicGetOk)

    server.send(got)
    receive_unwrap(server, m.BasicAck(delivery_tag=got.delivery_tag))

    assert not subsets_any_sendable(server, m.ChannelClose)


@pytest.mark.parametrize("close", DO_CHANNEL_CLOSE, ids=lambda c: c.__name__)
def test_no_ack_get_requeues_nothing(close: DoTeardown) -> None:
    server = channelled()

    get(server, no_ack=True)
    send_get_ok(server)
    close(server)
    reopen_channel(server)
    consume(server)

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


def test_no_ack_cancel_ok() -> None:
    server = consuming_no_ack()

    deliver(server, 1)
    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG))
    send_unwrap(server, m.BasicCancelOk, consumer_tag=CONSUMER_TAG)

    assert not subsets_any_sendable(server, m.BasicDeliver)
