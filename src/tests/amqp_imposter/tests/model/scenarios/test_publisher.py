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

from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    channelled,
    confirming,
    delivery_tags,
    enable_channel_error,
    lookup_sendables,
    message_content,
    publish,
    receive_unwrap,
    reconnect,
    reopen_channel,
    send_channel_error,
    send_unwrap,
    subsets_any_sendable,
)


def test_example_publisher() -> None:
    server = channelled()

    receive_unwrap(server, m.ConfirmSelect())
    send_unwrap(server, m.ConfirmSelectOk)

    hello = publish(server, body=b"hello", mandatory=True)
    publish(server, body=b"word")

    assert delivery_tags(server, m.BasicAck) == {
        (1, False),
        (2, False),
        (1, True),
        (2, True),
    }
    assert delivery_tags(server, m.BasicNack) == {
        (1, False),
        (2, False),
        (1, True),
        (2, True),
    }

    returned = send_unwrap(server, m.BasicReturn)

    assert returned.reply_code == mf.ReplyCode.UNROUTABLE
    assert message_content(returned) == message_content(hello)

    assert delivery_tags(server, m.BasicAck) == {
        (1, False),
        (2, False),
        (1, True),
        (2, True),
    }
    assert delivery_tags(server, m.BasicNack) == {(2, False)}
    assert not subsets_any_sendable(server, m.BasicReturn)

    send_unwrap(server, m.BasicAck, delivery_tag=1, multiple=False)

    assert delivery_tags(server, m.BasicAck) == {(2, False), (2, True)}
    assert delivery_tags(server, m.BasicNack) == {(2, False), (2, True)}

    send_unwrap(server, m.BasicAck, delivery_tag=2, multiple=False)

    assert lookup_sendables(server, m.BasicAck) == ()
    assert lookup_sendables(server, m.BasicNack) == ()

    publish(server, body=b"hello")

    assert delivery_tags(server, m.BasicAck) == {(3, False), (3, True)}
    assert delivery_tags(server, m.BasicNack) == {(3, False), (3, True)}


def test_example_resilient_publisher() -> None:
    server = confirming()

    publish(server, body=b"one")
    send_unwrap(server, m.BasicAck, delivery_tag=1, multiple=False)
    publish(server, body=b"two")
    enable_channel_error(server)
    send_channel_error(server)

    assert not subsets_any_sendable(server, m.BasicAck)
    assert not subsets_any_sendable(server, m.BasicNack)

    receive_unwrap(server, m.ChannelCloseOk())
    reopen_channel(server)
    receive_unwrap(server, m.ConfirmSelect())
    send_unwrap(server, m.ConfirmSelectOk)

    publish(server, body=b"two")

    assert delivery_tags(server, m.BasicAck) == {(1, False), (1, True)}

    send_unwrap(server, m.BasicAck, delivery_tag=1, multiple=False)
    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.CONNECTION_FORCED)

    assert server.sendables() == ()

    receive_unwrap(server, m.ConnectionCloseOk())
    reconnect(server)
    receive_unwrap(server, m.ConfirmSelect())
    send_unwrap(server, m.ConfirmSelectOk)

    publish(server, body=b"three")

    assert delivery_tags(server, m.BasicAck) == {(1, False), (1, True)}
