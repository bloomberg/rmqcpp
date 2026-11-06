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
    QUEUE,
    QUEUE_ARGUMENTS,
    ROUTING_KEY,
    ErrorReply,
    channel_errors,
    channelled,
    connection_errors,
    error_replies_to,
    lookup_unique_sendable,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)


def test_no_declare_ok_before_declare() -> None:
    assert not subsets_any_sendable(channelled(), m.QueueDeclareOk)
    assert not subsets_any_sendable(channelled(), m.ExchangeDeclareOk)
    assert not subsets_any_sendable(channelled(), m.QueueBindOk)


def test_declare_bind() -> None:
    server = channelled()

    receive_unwrap(server, m.ExchangeDeclare(exchange=EXCHANGE))
    send_unwrap(server, m.ExchangeDeclareOk)

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))

    assert lookup_unique_sendable(server, m.QueueDeclareOk).queue == QUEUE

    send_unwrap(server, m.QueueDeclareOk)

    receive_unwrap(
        server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, routing_key=ROUTING_KEY)
    )
    send_unwrap(server, m.QueueBindOk)

    assert not subsets_any_sendable(server, m.QueueBindOk)


def test_queue_without_exchange() -> None:
    # we don't model topology, so every queue declare can be answered with ok
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))

    assert lookup_unique_sendable(server, m.QueueDeclareOk).queue == QUEUE


def test_bind_without_queue() -> None:
    # we don't model topology, so every queue bind can be answered with ok
    server = channelled()

    receive_unwrap(
        server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, routing_key=ROUTING_KEY)
    )

    assert subsets_any_sendable(server, m.QueueBindOk)


def test_declare_anonymous_queue() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=""))

    assert lookup_unique_sendable(server, m.QueueDeclareOk).queue


def test_no_declare_ok_if_nowait() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))

    assert not subsets_any_sendable(server, m.QueueDeclareOk)


@pytest.mark.parametrize("no_wait", (False, True), ids=("wait", "no_wait"))
@pytest.mark.parametrize(
    ("passive", "errors"),
    (
        (
            True,
            channel_errors(
                mf.ReplyCode.NOT_FOUND,
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.RESOURCE_LOCKED,
            ),
        ),
        (
            False,
            channel_errors(
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.RESOURCE_LOCKED,
                mf.ReplyCode.PRECONDITION_FAILED,
            )
            | connection_errors(mf.ReplyCode.RESOURCE_ERROR),
        ),
    ),
    ids=("passive", "active"),
)
def test_declare_optional_errors(
    passive: bool, no_wait: bool, errors: set[ErrorReply]
) -> None:
    server = channelled()

    receive_unwrap(
        server, m.QueueDeclare(queue=QUEUE, passive=passive, no_wait=no_wait)
    )

    assert error_replies_to(server, m.QueueDeclare) == errors


@pytest.mark.parametrize("no_wait", (False, True), ids=("wait", "no_wait"))
def test_bind_optional_errors(no_wait: bool) -> None:
    server = channelled()

    receive_unwrap(server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, no_wait=no_wait))

    assert error_replies_to(server, m.QueueBind) == channel_errors(
        mf.ReplyCode.NOT_FOUND,
        mf.ReplyCode.ACCESS_REFUSED,
        mf.ReplyCode.RESOURCE_LOCKED,
    ) | connection_errors(mf.ReplyCode.RESOURCE_ERROR)


def test_no_bind_ok_if_nowait() -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.QueueBind(
            queue=QUEUE, exchange=EXCHANGE, routing_key=ROUTING_KEY, no_wait=True
        ),
    )

    assert not subsets_any_sendable(server, m.QueueBindOk)


def test_nowait_declare_then_bind_ok() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))
    receive_unwrap(
        server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, routing_key=ROUTING_KEY)
    )

    assert not subsets_any_sendable(server, m.QueueDeclareOk)
    assert subsets_any_sendable(server, m.QueueBindOk)


def test_declare_with_arguments() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, arguments=QUEUE_ARGUMENTS))

    assert subsets_any_sendable(server, m.QueueDeclareOk)


def test_bind_with_arguments() -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.QueueBind(
            queue=QUEUE,
            exchange=EXCHANGE,
            arguments=QUEUE_ARGUMENTS,  # the exact keys are not important
        ),
    )

    assert subsets_any_sendable(server, m.QueueBindOk)
