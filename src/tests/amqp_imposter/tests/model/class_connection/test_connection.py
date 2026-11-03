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

from amqp_imposter.model import (
    ModelServer,
    Unrepresentable,
)
from amqp_imposter.model import methods as m

from ..helpers import (
    DO_CONNECTION_CLOSE,
    GRACEFUL_CHANNEL_CLOSE,
    GRACEFUL_CONNECTION_CLOSE,
    DoTeardown,
    channelled,
    connected,
    greeted,
    lookup_unique_sendable,
    receive_unwrap,
    send_unwrap,
    start_ok_of,
    subsets_any_sendable,
    tune_ok_of,
    tuned,
    tuning,
)


def test_open_and_close() -> None:
    server = ModelServer()

    server.receive_protocol_header()
    receive_unwrap(server, start_ok_of(send_unwrap(server, m.ConnectionStart)))

    receive_unwrap(server, tune_ok_of(send_unwrap(server, m.ConnectionTune)))
    receive_unwrap(server, m.ConnectionOpen())

    send_unwrap(server, m.ConnectionOpenOk)
    receive_unwrap(server, m.ChannelOpen())

    send_unwrap(server, m.ChannelOpenOk)
    receive_unwrap(server, GRACEFUL_CHANNEL_CLOSE)

    send_unwrap(server, m.ChannelCloseOk)
    receive_unwrap(server, GRACEFUL_CONNECTION_CLOSE)

    send_unwrap(server, m.ConnectionCloseOk)

    assert server.sendables() == ()


def test_start_has_capabilities() -> None:
    start = lookup_unique_sendable(greeted(), m.ConnectionStart)

    assert start.publisher_confirms
    assert start.basic_nack
    assert start.consumer_cancel_notify


def test_start_is_alone_in_bag() -> None:
    assert len(greeted().sendables()) == 1


def test_heartbeat_is_zero() -> None:
    assert lookup_unique_sendable(tuning(), m.ConnectionTune).heartbeat == 0


def test_tune_ok_is_noop() -> None:
    server = tuned()

    assert not subsets_any_sendable(server, m.ConnectionTune)
    assert not subsets_any_sendable(server, m.ConnectionOpenOk)


def test_no_open_ok_before_open() -> None:
    assert not subsets_any_sendable(tuned(), m.ConnectionOpenOk)


@pytest.mark.parametrize("close", DO_CONNECTION_CLOSE, ids=lambda c: c.__name__)
def test_reopen_connection(close: DoTeardown) -> None:
    server = channelled()

    close(server)
    server.receive_protocol_header()

    assert subsets_any_sendable(server, m.ConnectionStart)


def test_simultaneous_connections_raise() -> None:
    server = connected()

    assert isinstance(server.receive_protocol_header(), Unrepresentable)


def test_dupe_header_raises() -> None:
    server = tuning()

    assert isinstance(server.receive_protocol_header(), Unrepresentable)
