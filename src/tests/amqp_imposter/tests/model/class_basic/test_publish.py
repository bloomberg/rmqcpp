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
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    EXCHANGE,
    QUEUE,
    ROUTING_KEY,
    ErrorReply,
    channel_errors,
    channelled,
    confirming,
    connection_errors,
    consuming,
    deliver,
    delivery_tags,
    error_replies_to,
    publish,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)


@pytest.mark.parametrize(
    ("exchange", "errors"),
    (
        (
            EXCHANGE,
            channel_errors(
                mf.ReplyCode.NOT_FOUND,
                mf.ReplyCode.ACCESS_REFUSED,
                mf.ReplyCode.CONTENT_TOO_LARGE,
            ),
        ),
        (
            "",
            channel_errors(mf.ReplyCode.ACCESS_REFUSED, mf.ReplyCode.CONTENT_TOO_LARGE),
        ),
    ),
    ids=("named", "default"),
)
def test_publish_error_options(exchange: str, errors: set[ErrorReply]) -> None:
    server = channelled()

    receive_unwrap(server, m.BasicPublish(exchange=exchange, routing_key=ROUTING_KEY))

    assert error_replies_to(server, m.BasicPublish) == errors


def test_publish_error_closes_channel() -> None:
    server = confirming()

    publish(server)
    send_unwrap(server, m.ChannelClose, reply_code=mf.ReplyCode.NOT_FOUND)

    assert not subsets_any_sendable(server, m.BasicAck)
    assert not isinstance(server.receive(m.ChannelCloseOk()), Unrepresentable)


@pytest.mark.parametrize(
    "confirm", (m.BasicAck, m.BasicNack), ids=lambda kind: kind.__name__
)
def test_confirm_discards_publish_error(
    confirm: type[m.BasicAck | m.BasicNack],
) -> None:
    server = confirming()

    publish(server)

    assert error_replies_to(server, m.BasicPublish)

    send_unwrap(server, confirm, delivery_tag=1, multiple=False)

    assert not error_replies_to(server, m.BasicPublish)


def test_later_confirm_discards_publish_error() -> None:
    server = confirming()

    publish(server, body=b"one")
    publish(server, body=b"two")

    assert error_replies_to(server, m.BasicPublish)

    send_unwrap(server, m.BasicAck, delivery_tag=2, multiple=False)

    assert not error_replies_to(server, m.BasicPublish)


def test_earlier_confirm_keeps_publish_error() -> None:
    server = confirming()

    publish(server, body=b"one")
    publish(server, body=b"two")
    send_unwrap(server, m.BasicAck, delivery_tag=1, multiple=False)

    assert error_replies_to(server, m.BasicPublish)


def test_return_discards_publish_error() -> None:
    server = channelled()

    publish(server, mandatory=True)

    assert error_replies_to(server, m.BasicPublish)

    send_unwrap(server, m.BasicReturn)

    assert not error_replies_to(server, m.BasicPublish)


def test_reply_discards_publish_error() -> None:
    server = channelled()

    publish(server)
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))

    assert error_replies_to(server, m.BasicPublish)

    send_unwrap(server, m.QueueDeclareOk)

    assert not error_replies_to(server, m.BasicPublish)


def test_delivery_keeps_publish_error() -> None:
    server = consuming()

    publish(server)
    deliver(server, 1)

    assert error_replies_to(server, m.BasicPublish)


def test_publish_error_ordered() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    publish(server)

    assert not error_replies_to(server, m.BasicPublish)

    send_unwrap(server, m.QueueDeclareOk)

    assert error_replies_to(server, m.BasicPublish)


def test_immediate_not_implemented() -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.BasicPublish(exchange=EXCHANGE, routing_key=ROUTING_KEY, immediate=True),
    )

    assert error_replies_to(server, m.BasicPublish) == connection_errors(
        mf.ReplyCode.NOT_IMPLEMENTED
    )


def test_out_of_order_confirm() -> None:
    server = confirming()

    publish(server, body=b"one")
    publish(server, body=b"two")
    publish(server, body=b"three")

    send_unwrap(server, m.BasicAck, delivery_tag=2, multiple=False)

    assert delivery_tags(server, m.BasicAck) == {
        (1, False),
        (1, True),
        (3, False),
        (3, True),
    }
    assert delivery_tags(server, m.BasicNack) == {
        (1, False),
        (1, True),
        (3, False),
        (3, True),
    }
