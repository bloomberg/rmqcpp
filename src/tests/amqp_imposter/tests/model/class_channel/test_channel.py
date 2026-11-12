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
    DO_CHANNEL_CLOSE,
    DoTeardown,
    channelled,
    connected,
    connection_errors,
    error_replies_to,
    receive_unwrap,
    subsets_any_sendable,
)


def test_no_open_ok_before_open() -> None:
    assert not subsets_any_sendable(connected(), m.ChannelOpenOk)


def test_open_optional_errors() -> None:
    server = connected()

    receive_unwrap(server, m.ChannelOpen())

    assert error_replies_to(server, m.ChannelOpen) == connection_errors(
        mf.ReplyCode.RESOURCE_ERROR
    )


@pytest.mark.parametrize("close", DO_CHANNEL_CLOSE, ids=lambda c: c.__name__)
def test_reopen_channel(close: DoTeardown) -> None:
    server = channelled()

    close(server)
    receive_unwrap(server, m.ChannelOpen())

    assert subsets_any_sendable(server, m.ChannelOpenOk)


def test_reopen_open_channel_closes_connection() -> None:
    server = channelled()

    receive_unwrap(server, m.ChannelOpen())

    assert error_replies_to(server, m.ChannelOpen) == connection_errors(
        mf.ReplyCode.CHANNEL_ERROR
    )


@pytest.mark.parametrize("close", DO_CHANNEL_CLOSE, ids=lambda c: c.__name__)
def test_method_on_closed_channel_closes_connection(close: DoTeardown) -> None:
    server = channelled()

    close(server)
    receive_unwrap(server, m.ConfirmSelect())

    assert error_replies_to(server, m.ConfirmSelect) == connection_errors(
        mf.ReplyCode.CHANNEL_ERROR
    )
