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
    CONNECTING_STATES,
    DO_CONNECTION_CLOSE,
    GRACEFUL_CONNECTION_CLOSE,
    DoTeardown,
    MakeServer,
    channelled,
    closed,
    confirming,
    consume,
    consuming,
    deliver,
    lookup_sendables,
    message_content,
    publish,
    receive_unwrap,
    reconnect,
    send_unwrap,
    subsets_any_sendable,
)


def test_teardown() -> None:
    server = channelled()

    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.CONNECTION_FORCED)

    assert server.sendables() == ()

    receive_unwrap(server, m.ConnectionCloseOk())

    assert server.sendables() == ()


def test_closed_connection_has_nothing_to_send() -> None:
    assert closed().sendables() == ()


@pytest.mark.parametrize("make_server", CONNECTING_STATES, ids=lambda b: b.__name__)
def test_no_unprompted_close_before_open(make_server: MakeServer) -> None:
    assert not subsets_any_sendable(
        make_server(),
        m.ConnectionClose,
        class_id=mf.UNPROMPTED_CLASS_ID,
        method_id=mf.UNPROMPTED_METHOD_ID,
    )


@pytest.mark.parametrize("make_server", CONNECTED_STATES, ids=lambda b: b.__name__)
def test_has_close_after_open(make_server: MakeServer) -> None:
    assert subsets_any_sendable(make_server(), m.ConnectionClose)


@pytest.mark.parametrize("make_server", CONNECTED_STATES, ids=lambda b: b.__name__)
def test_unprompted_close_codes(make_server: MakeServer) -> None:
    closes = lookup_sendables(
        make_server(),
        m.ConnectionClose,
        class_id=mf.UNPROMPTED_CLASS_ID,
        method_id=mf.UNPROMPTED_METHOD_ID,
    )

    codes = sorted(mf.assert_concrete(close.reply_code) for close in closes)

    assert codes == sorted(mf.UNPROMPTED_ERRORS)


@pytest.mark.parametrize("close", DO_CONNECTION_CLOSE, ids=lambda c: c.__name__)
def test_connection_close_requeues_unacked(close: DoTeardown) -> None:
    server = consuming(prefetch_count=2)

    delivered = tuple(deliver(server, delivery_tag) for delivery_tag in (1, 2))

    close(server)
    reconnect(server)
    consume(server)

    redeliveries = lookup_sendables(server, m.BasicDeliver, redelivered=True)

    assert {message_content(d) for d in redeliveries} == {
        message_content(d) for d in delivered
    }


def test_received_methods_discarded_after_connection_close() -> None:
    server = channelled()

    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.CONNECTION_FORCED)
    receive_unwrap(server, m.ConfirmSelect())
    receive_unwrap(server, m.ChannelOpen())
    receive_unwrap(server, m.BasicAck(delivery_tag=99))
    publish(server, mandatory=True)

    assert server.sendables() == ()

    receive_unwrap(server, m.ConnectionCloseOk())
    reconnect(server)

    assert not subsets_any_sendable(server, m.ConfirmSelectOk)
    assert not subsets_any_sendable(server, m.ChannelOpenOk)
    assert not subsets_any_sendable(server, m.BasicReturn)


def test_connection_close_bags_close_ok() -> None:
    server = channelled()

    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.CONNECTION_FORCED)
    receive_unwrap(server, GRACEFUL_CONNECTION_CLOSE)
    send_unwrap(server, m.ConnectionCloseOk)

    assert not subsets_any_sendable(server, m.ConnectionClose)


def test_connection_close_destroys_everything() -> None:
    server = confirming()

    publish(server)
    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.CONNECTION_FORCED)

    assert server.sendables() == ()


def both_sides_close_connection() -> ModelServer:
    server = channelled()
    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.CONNECTION_FORCED)
    receive_unwrap(server, GRACEFUL_CONNECTION_CLOSE)
    return server


@pytest.mark.xfail(
    strict=True, reason="ideally both sides send ok when intending to close connection"
)
def test_both_sides_close_connection_can_accept_close_ok() -> None:
    server = both_sides_close_connection()
    send_unwrap(server, m.ConnectionCloseOk)

    receive_unwrap(server, m.ConnectionCloseOk())


@pytest.mark.xfail(
    strict=True, reason="ideally both sides send ok when intending to close connection"
)
def test_both_sides_close_connection_owes_close_ok() -> None:
    server = both_sides_close_connection()
    receive_unwrap(server, m.ConnectionCloseOk())

    assert subsets_any_sendable(server, m.ConnectionCloseOk)
