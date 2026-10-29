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
    EXCHANGE,
    QUEUE_ARGUMENTS,
    ROUTING_KEY,
    channel_errors,
    channelled,
    connection_errors,
    error_replies_to,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)

SOURCE_EXCHANGE = EXCHANGE + "source"


def test_no_bind_ok_before_bind() -> None:
    assert not subsets_any_sendable(channelled(), m.ExchangeBindOk)


def test_declare_bind() -> None:
    server = channelled()

    receive_unwrap(server, m.ExchangeDeclare(exchange=EXCHANGE))
    send_unwrap(server, m.ExchangeDeclareOk)

    receive_unwrap(server, m.ExchangeDeclare(exchange=SOURCE_EXCHANGE))
    send_unwrap(server, m.ExchangeDeclareOk)

    receive_unwrap(
        server,
        m.ExchangeBind(
            destination=EXCHANGE, source=SOURCE_EXCHANGE, routing_key=ROUTING_KEY
        ),
    )
    send_unwrap(server, m.ExchangeBindOk)

    assert not subsets_any_sendable(server, m.ExchangeBindOk)


def test_exchange_bind_cyclical() -> None:
    server = channelled()

    receive_unwrap(server, m.ExchangeDeclare(exchange=EXCHANGE))
    send_unwrap(server, m.ExchangeDeclareOk)

    receive_unwrap(
        server,
        m.ExchangeBind(destination=EXCHANGE, source=EXCHANGE, routing_key=ROUTING_KEY),
    )
    send_unwrap(server, m.ExchangeBindOk)

    assert not subsets_any_sendable(server, m.ChannelClose)


def test_bind_without_exchanges() -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.ExchangeBind(
            destination=EXCHANGE, source=SOURCE_EXCHANGE, routing_key=ROUTING_KEY
        ),
    )

    assert subsets_any_sendable(server, m.ExchangeBindOk)


def test_no_bind_ok_if_nowait() -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.ExchangeBind(
            destination=EXCHANGE,
            source=SOURCE_EXCHANGE,
            routing_key=ROUTING_KEY,
            no_wait=True,
        ),
    )

    assert not subsets_any_sendable(server, m.ExchangeBindOk)


@pytest.mark.parametrize("no_wait", (False, True), ids=("wait", "no_wait"))
def test_bind_optional_errors(no_wait: bool) -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.ExchangeBind(destination=EXCHANGE, source=SOURCE_EXCHANGE, no_wait=no_wait),
    )

    assert error_replies_to(server, m.ExchangeBind) == channel_errors(
        mf.ReplyCode.NOT_FOUND, mf.ReplyCode.ACCESS_REFUSED
    ) | connection_errors(mf.ReplyCode.RESOURCE_ERROR)


def test_bind_with_arguments() -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.ExchangeBind(
            destination=EXCHANGE,
            source=SOURCE_EXCHANGE,
            arguments=QUEUE_ARGUMENTS,
        ),
    )

    assert subsets_any_sendable(server, m.ExchangeBindOk)
