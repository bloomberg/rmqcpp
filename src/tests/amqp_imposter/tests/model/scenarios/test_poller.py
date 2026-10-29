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

from amqp_imposter.model import methods as m

from ..helpers import (
    channelled,
    get,
    lookup_unique_sendable,
    message_content,
    receive_unwrap,
    send_get_ok,
    send_unwrap,
    subsets_any_sendable,
)


def test_example_poller() -> None:
    server = channelled()

    assert not subsets_any_sendable(server, m.BasicGetOk)

    get(server)

    assert lookup_unique_sendable(server, m.BasicGetOk).delivery_tag == 1

    send_unwrap(server, m.BasicGetEmpty)
    get(server)

    first = send_get_ok(server, body=b"one")

    assert first.delivery_tag == 1

    receive_unwrap(server, m.BasicAck(delivery_tag=1))
    get(server)

    second = send_get_ok(server, body=b"two")

    assert second.delivery_tag == 2

    receive_unwrap(server, m.BasicReject(delivery_tag=2, requeue=True))
    get(server)

    again = lookup_unique_sendable(server, m.BasicGetOk, redelivered=True)

    assert message_content(again) == message_content(second)
    assert again.delivery_tag == 3

    server.send(again)
    receive_unwrap(server, m.BasicAck(delivery_tag=3))
    get(server)

    assert not subsets_any_sendable(server, m.BasicGetOk, redelivered=True)

    send_unwrap(server, m.BasicGetEmpty)

    assert not subsets_any_sendable(server, m.ChannelClose)
