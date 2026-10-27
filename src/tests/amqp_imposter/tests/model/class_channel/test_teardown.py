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

from amqp_imposter.model import ModelServer
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    CONNECTED_STATES,
    DO_CHANNEL_CLOSE,
    GRACEFUL_CHANNEL_CLOSE,
    QUEUE,
    DoTeardown,
    MakeServer,
    channelled,
    confirming,
    consume,
    consuming,
    deliver,
    enable_channel_error,
    lookup_sendables,
    message_content,
    publish,
    receive_unwrap,
    reopen_channel,
    send_channel_error,
    send_unwrap,
    subsets_any_sendable,
)


@pytest.mark.parametrize("make_server", CONNECTED_STATES, ids=lambda b: b.__name__)
def test_no_unprompted_channel_close(make_server: MakeServer) -> None:
    assert not subsets_any_sendable(
        make_server(),
        m.ChannelClose,
        class_id=mf.UNPROMPTED_CLASS_ID,
        method_id=mf.UNPROMPTED_METHOD_ID,
    )


def test_channel_close_awaits_close_ok() -> None:
    server = consuming()

    enable_channel_error(server)
    send_channel_error(server)

    assert not subsets_any_sendable(server, m.BasicDeliver)
    assert not subsets_any_sendable(server, m.ChannelClose)
    assert subsets_any_sendable(server, m.ConnectionClose)


def test_channel_close_destroys_confirms() -> None:
    server = confirming()

    publish(server)
    enable_channel_error(server)
    send_channel_error(server)

    assert not subsets_any_sendable(server, m.BasicAck)
    assert not subsets_any_sendable(server, m.BasicNack)


def test_close_preempts_confirms() -> None:
    server = confirming()

    publish(server)
    enable_channel_error(server)

    assert subsets_any_sendable(server, m.ChannelClose)


@pytest.mark.parametrize("close", DO_CHANNEL_CLOSE, ids=lambda c: c.__name__)
def test_channel_close_requeues_unacked(close: DoTeardown) -> None:
    server = consuming(prefetch_count=2)

    delivered = tuple(deliver(server, delivery_tag) for delivery_tag in (1, 2))

    close(server)
    reopen_channel(server)
    consume(server)

    redeliveries = lookup_sendables(server, m.BasicDeliver, redelivered=True)

    assert {message_content(d) for d in redeliveries} == {
        message_content(d) for d in delivered
    }


def test_received_methods_discarded_after_channel_close() -> None:
    server = channelled()

    enable_channel_error(server)
    send_channel_error(server)

    closing = server.sendables()

    receive_unwrap(server, m.ConfirmSelect())
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(server, m.BasicAck(delivery_tag=99))
    publish(server, mandatory=True)

    assert server.sendables() == closing

    receive_unwrap(server, m.ChannelCloseOk())
    reopen_channel(server)

    assert not subsets_any_sendable(server, m.ConfirmSelectOk)
    assert not subsets_any_sendable(server, m.QueueDeclareOk)
    assert not subsets_any_sendable(server, m.BasicReturn)


def test_received_methods_discarded_while_the_close_is_owed() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    enable_channel_error(server)
    receive_unwrap(server, m.ConfirmSelect())

    assert not subsets_any_sendable(server, m.ConfirmSelectOk)


def test_channel_close_bags_close_ok() -> None:
    server = channelled()

    enable_channel_error(server)
    send_channel_error(server)
    receive_unwrap(server, GRACEFUL_CHANNEL_CLOSE)

    assert subsets_any_sendable(server, m.ChannelCloseOk)
    assert not subsets_any_sendable(server, m.ChannelClose)


def both_sides_close_channel() -> ModelServer:
    server = channelled()
    enable_channel_error(server)
    send_channel_error(server)
    receive_unwrap(server, GRACEFUL_CHANNEL_CLOSE)
    return server


@pytest.mark.xfail(
    strict=True, reason="ideally both sides send ok when intending to close channel"
)
def test_both_sides_close_channel_can_accept_close_ok() -> None:
    # if the consumer is intending to close too, and we send our Ok, we still need to accept their Ok
    server = both_sides_close_channel()
    send_unwrap(server, m.ChannelCloseOk)

    receive_unwrap(server, m.ChannelCloseOk())


@pytest.mark.xfail(
    strict=True, reason="ideally both sides send ok when intending to close channel"
)
def test_both_sides_close_channel_closes_owe_close_ok() -> None:
    # if the consumer is intending to close too, and we get their Ok, we still need to send our Ok
    server = both_sides_close_channel()
    receive_unwrap(server, m.ChannelCloseOk())

    assert subsets_any_sendable(server, m.ChannelCloseOk)
