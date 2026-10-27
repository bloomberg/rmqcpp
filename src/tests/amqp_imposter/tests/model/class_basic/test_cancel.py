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

from amqp_imposter.model import Unrepresentable
from amqp_imposter.model import methods as m

from ..helpers import (
    CONSUMER_TAG,
    DO_CANCEL,
    OTHER_CONSUMER_TAG,
    QUEUE,
    DoTeardown,
    consume,
    consuming,
    deliver,
    receive_cancel,
    receive_unwrap,
    send_cancel,
    send_unwrap,
    subsets_any_sendable,
)


def test_no_cancel_ok_before_cancel() -> None:
    assert not subsets_any_sendable(consuming(), m.BasicCancelOk)


def test_server_cancel_stops_deliveries() -> None:
    server = consuming()

    send_cancel(server)

    assert not subsets_any_sendable(server, m.BasicDeliver)
    assert not subsets_any_sendable(server, m.BasicCancel)


def test_server_cancel_leaves_the_channel_open() -> None:
    server = consuming()

    send_cancel(server)
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))

    assert subsets_any_sendable(server, m.QueueDeclareOk)


@pytest.mark.parametrize("cancel", DO_CANCEL, ids=lambda c: c.__name__)
def test_cancel_requeues_nothing(cancel: DoTeardown) -> None:
    server = consuming(prefetch_count=0)

    deliver(server, 1)
    deliver(server, 2)
    cancel(server)
    consume(server)  # reconsume

    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


@pytest.mark.parametrize("cancel", DO_CANCEL, ids=lambda c: c.__name__)
def test_ack_after_cancel(cancel: DoTeardown) -> None:
    server = consuming(prefetch_count=0)

    deliver(server, 1)
    deliver(server, 2)
    cancel(server)
    receive_unwrap(server, m.BasicAck(delivery_tag=2, multiple=True))

    assert not subsets_any_sendable(server, m.ChannelClose)


def test_client_cancel_during_server_cancel() -> None:
    server = consuming()

    send_cancel(server)
    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG))

    send_unwrap(server, m.BasicCancelOk, consumer_tag=CONSUMER_TAG)


def test_reconsume_and_client_cancel_during_server_cancel() -> None:
    server = consuming()

    send_cancel(server)
    receive_unwrap(server, m.BasicConsume(queue=QUEUE, consumer_tag=OTHER_CONSUMER_TAG))
    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG))
    send_unwrap(server, m.BasicConsumeOk, consumer_tag=OTHER_CONSUMER_TAG)
    send_unwrap(server, m.BasicCancelOk, consumer_tag=CONSUMER_TAG)

    assert subsets_any_sendable(server, m.BasicDeliver, consumer_tag=OTHER_CONSUMER_TAG)


@pytest.mark.parametrize("consumer_tag", ("", "other"), ids=("empty", "unknown"))
def test_client_cancel_during_server_cancel_bad_tag(consumer_tag: str) -> None:
    server = consuming()

    send_unwrap(server, m.BasicCancel, consumer_tag=CONSUMER_TAG, no_wait=True)
    cancelled = server.receive(m.BasicCancel(consumer_tag=consumer_tag))

    assert isinstance(cancelled, Unrepresentable)


def test_double_client_cancel() -> None:
    server = consuming()

    receive_cancel(server)

    cancelled = server.receive(m.BasicCancel(consumer_tag=CONSUMER_TAG))

    assert isinstance(cancelled, Unrepresentable)


@pytest.mark.parametrize("consumer_tag", ("", "other"), ids=("empty", "unknown"))
def test_cancel_unknown_tag(consumer_tag: str) -> None:
    # RabbitMQ seems to reply CancelOk and cancel nothing
    # but this is a client error so let's call it Unrepresentable
    server = consuming()

    cancelled = server.receive(m.BasicCancel(consumer_tag=consumer_tag))

    assert isinstance(cancelled, Unrepresentable)


def test_reconsume_after_server_cancel() -> None:
    server = consuming()

    send_cancel(server)
    receive_unwrap(server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG))
    send_unwrap(server, m.BasicConsumeOk, consumer_tag=CONSUMER_TAG)

    assert subsets_any_sendable(server, m.BasicDeliver, consumer_tag=CONSUMER_TAG)


def test_server_cancel_is_nowait() -> None:
    server = consuming()

    assert not subsets_any_sendable(server, m.BasicCancel, no_wait=False)

    send_unwrap(server, m.BasicCancel, consumer_tag=CONSUMER_TAG, no_wait=True)


def test_client_cancel_ok() -> None:
    server = consuming()

    receive_cancel(server)

    assert not subsets_any_sendable(server, m.BasicCancelOk)


def test_no_deliveries_after_cancel_ok() -> None:
    server = consuming(prefetch_count=0)

    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG))
    deliver(server, 1)
    send_unwrap(server, m.BasicCancelOk, consumer_tag=CONSUMER_TAG)

    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_client_cancel_preempts_the_server_cancel() -> None:
    server = consuming()

    receive_cancel(server)

    assert not subsets_any_sendable(server, m.BasicCancel)


def test_no_cancel_ok_if_nowait() -> None:
    server = consuming()

    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG, no_wait=True))

    assert not subsets_any_sendable(server, m.BasicCancelOk)
    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_client_cancel_leaves_the_channel_open() -> None:
    server = consuming()

    receive_cancel(server)
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))

    assert subsets_any_sendable(server, m.QueueDeclareOk)
