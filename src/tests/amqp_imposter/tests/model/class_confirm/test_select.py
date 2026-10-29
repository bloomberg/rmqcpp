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
    QUEUE,
    channelled,
    confirming,
    delivery_tags,
    lookup_sendables,
    publish,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)


def test_no_select_ok_if_nowait() -> None:
    server = channelled()

    receive_unwrap(server, m.ConfirmSelect(no_wait=True))

    assert not subsets_any_sendable(server, m.ConfirmSelectOk)


def test_publish_before_select_ok() -> None:
    server = channelled()

    receive_unwrap(server, m.ConfirmSelect())
    publish(server, body=b"early")

    # the broker doesn't do confirms until it's sent the Confirm.SelectOk
    assert not subsets_any_sendable(server, m.BasicAck)

    send_unwrap(server, m.ConfirmSelectOk)

    assert subsets_any_sendable(server, m.BasicAck, delivery_tag=1)

    publish(server, body=b"late")

    assert subsets_any_sendable(server, m.BasicAck, delivery_tag=2)


def test_return_before_select_ok() -> None:
    server = channelled()

    receive_unwrap(server, m.ConfirmSelect())
    publish(server, body=b"early", mandatory=True)

    assert not subsets_any_sendable(server, m.BasicReturn)

    send_unwrap(server, m.ConfirmSelectOk)

    assert subsets_any_sendable(server, m.BasicReturn)


def test_second_select() -> None:
    server = confirming()

    publish(server, body=b"early")
    receive_unwrap(server, m.ConfirmSelect())

    assert subsets_any_sendable(server, m.BasicAck, delivery_tag=1)

    send_unwrap(server, m.ConfirmSelectOk)
    publish(server, body=b"late")

    assert subsets_any_sendable(server, m.BasicAck, delivery_tag=2)


def test_nowait_select_orthogonal_to_acknowledgements() -> None:
    server = channelled()

    receive_unwrap(server, m.ConfirmSelect(no_wait=True))
    publish(server)

    assert subsets_any_sendable(server, m.BasicAck, delivery_tag=1)
    assert subsets_any_sendable(server, m.BasicNack, delivery_tag=1)


def test_confirm_deferred_until_declare_ok() -> None:
    server = confirming()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    publish(server, body=b"after")

    assert not subsets_any_sendable(server, m.BasicAck)

    send_unwrap(server, m.QueueDeclareOk)

    assert subsets_any_sendable(server, m.BasicAck, delivery_tag=1)


def test_return_deferred_until_declare_ok() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    publish(server, body=b"after", mandatory=True)

    assert not subsets_any_sendable(server, m.BasicReturn)

    send_unwrap(server, m.QueueDeclareOk)

    assert subsets_any_sendable(server, m.BasicReturn)


def test_confirm_not_deferred_until_declare_ok() -> None:
    server = confirming()

    publish(server, body=b"before")
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))

    assert subsets_any_sendable(server, m.BasicAck, delivery_tag=1)
    assert subsets_any_sendable(server, m.QueueDeclareOk)


def test_many_deferred_publishes_become_sendable() -> None:
    server = confirming()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    publish(server, body=b"one")
    publish(server, body=b"two")
    publish(server, body=b"three")
    send_unwrap(server, m.QueueDeclareOk)

    assert delivery_tags(server, m.BasicAck) >= {(1, False), (2, False), (3, False)}


def test_many_deferred_returns_become_sendable() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two", mandatory=True)
    send_unwrap(server, m.QueueDeclareOk)

    assert len(lookup_sendables(server, m.BasicReturn)) == 2
