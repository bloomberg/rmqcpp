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
    ErrorReply,
    channel_errors,
    channelled,
    confirming,
    connection_errors,
    error_replies_to,
    publish,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)


@pytest.mark.parametrize("no_wait", (False, True), ids=("wait", "no_wait"))
@pytest.mark.parametrize(
    ("passive", "errors"),
    (
        (
            True,
            channel_errors(mf.ReplyCode.NOT_FOUND, mf.ReplyCode.ACCESS_REFUSED),
        ),
        (
            False,
            channel_errors(
                mf.ReplyCode.ACCESS_REFUSED, mf.ReplyCode.PRECONDITION_FAILED
            )
            | connection_errors(mf.ReplyCode.NOT_ALLOWED, mf.ReplyCode.RESOURCE_ERROR),
        ),
    ),
    ids=("passive", "active"),
)
def test_declare_optional_errors(
    passive: bool, no_wait: bool, errors: set[ErrorReply]
) -> None:
    server = channelled()

    receive_unwrap(
        server, m.ExchangeDeclare(exchange=EXCHANGE, passive=passive, no_wait=no_wait)
    )

    assert error_replies_to(server, m.ExchangeDeclare) == errors


def test_declare_error_closes_connection() -> None:
    server = channelled()

    receive_unwrap(server, m.ExchangeDeclare(exchange=EXCHANGE))
    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.NOT_ALLOWED)
    receive_unwrap(server, m.ConnectionCloseOk())

    assert server.receive_protocol_header() is None


def test_topology_orthogonal_to_routability() -> None:
    # we treat topology as volatile, so the return is always available
    # irrespective of whether we have declared the exchange or not
    # (N.b. in real life someone could also delete the exchange concurrently)
    server = confirming()

    receive_unwrap(server, m.ExchangeDeclare(exchange=EXCHANGE))
    send_unwrap(server, m.ExchangeDeclareOk)
    publish(server, mandatory=True)  # the only message on this channel

    assert subsets_any_sendable(server, m.BasicReturn)  # the channel has a return


def test_no_declare_ok_if_nowait() -> None:
    server = channelled()

    receive_unwrap(server, m.ExchangeDeclare(exchange=EXCHANGE, no_wait=True))

    assert not subsets_any_sendable(server, m.ExchangeDeclareOk)


def test_nowait_declare_orthogonal_to_acknowledgements() -> None:
    server = confirming()

    receive_unwrap(server, m.ExchangeDeclare(exchange=EXCHANGE, no_wait=True))
    publish(server, mandatory=True)

    assert subsets_any_sendable(server, m.BasicReturn)
    assert subsets_any_sendable(server, m.BasicAck)


def test_declare_auto_delete_and_internal() -> None:
    server = channelled()

    receive_unwrap(
        server, m.ExchangeDeclare(exchange=EXCHANGE, auto_delete=True, internal=True)
    )

    assert subsets_any_sendable(server, m.ExchangeDeclareOk)


def test_declare_with_arguments() -> None:
    server = channelled()

    receive_unwrap(
        server, m.ExchangeDeclare(exchange=EXCHANGE, arguments=QUEUE_ARGUMENTS)
    )

    assert subsets_any_sendable(server, m.ExchangeDeclareOk)
