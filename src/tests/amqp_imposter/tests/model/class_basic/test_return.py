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
    QUEUE,
    channelled,
    confirming,
    delivery_tags,
    error_replies_to,
    lookup_sendables,
    lookup_unique_sendable,
    publish,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)


def test_return_destroys_corresponding_nack() -> None:
    server = confirming()

    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two", mandatory=True)
    publish(server, body=b"three", mandatory=True)
    publish(server, body=b"four", mandatory=True)

    server.send(lookup_unique_sendable(server, m.BasicReturn, body=b"two"))

    # once Return is sent, we can no longer Nack the message
    # nor with any multiple Nack that includes it
    assert delivery_tags(server, m.BasicNack) == {
        (1, False),
        (1, True),
        (3, False),
        (4, False),
    }
    # Ack is what's meant to come after Return
    assert delivery_tags(server, m.BasicAck) == {
        (1, False),
        (1, True),
        (2, False),
        (2, True),
        (3, False),
        (3, True),
        (4, False),
        (4, True),
    }


def test_return_all_destroys_all_nacks() -> None:
    server = confirming()

    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two", mandatory=True)
    publish(server, body=b"three", mandatory=True)

    for unconfirmed_states in lookup_sendables(server, m.BasicReturn):
        server.send(unconfirmed_states)

    assert delivery_tags(server, m.BasicNack) == set()
    assert delivery_tags(server, m.BasicAck) == {
        (1, False),
        (1, True),
        (2, False),
        (2, True),
        (3, False),
        (3, True),
    }


def test_new_publish_cannot_multiple_nack_past_a_return() -> None:
    server = confirming()

    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two")

    send_unwrap(server, m.BasicReturn)
    publish(server, body=b"three")

    assert subsets_any_sendable(server, m.BasicNack, delivery_tag=3, multiple=False)
    assert not subsets_any_sendable(server, m.BasicNack, delivery_tag=3, multiple=True)


def test_multiple_ack_destroys_corresponding_returns() -> None:
    server = confirming()

    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two", mandatory=True)

    send_unwrap(server, m.BasicAck, delivery_tag=2, multiple=True)

    assert not subsets_any_sendable(server, m.BasicReturn)


def test_multiple_ack_spares_uncovered_returns() -> None:
    server = confirming()

    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two", mandatory=True)

    send_unwrap(server, m.BasicAck, delivery_tag=1, multiple=True)

    assert len(lookup_sendables(server, m.BasicReturn)) == 1


def test_multiple_nack_cannot_span_a_returned_tag() -> None:
    server = confirming()

    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two")
    publish(server, body=b"three")

    send_unwrap(server, m.BasicReturn)

    assert delivery_tags(server, m.BasicNack) == {(2, False), (3, False)}
    assert delivery_tags(server, m.BasicAck) == {
        (1, False),
        (2, False),
        (3, False),
        (1, True),
        (2, True),
        (3, True),
    }


def test_multiple_ack_may_span_a_returned_tag() -> None:
    server = confirming()

    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two")

    send_unwrap(server, m.BasicReturn)
    send_unwrap(server, m.BasicAck, delivery_tag=2, multiple=True)

    assert delivery_tags(server, m.BasicAck) == set()


def test_return_has_cluster_id_void() -> None:
    server = channelled()

    publish(server, mandatory=True)

    returned = lookup_unique_sendable(server, m.BasicReturn)

    assert isinstance(returned.properties, mf.MessageProperties)
    assert returned.properties.cluster_id == mf.Void()


def test_identical_returns_for_different_tags() -> None:
    server = channelled()

    publish(server, body=b"same", mandatory=True)
    publish(server, body=b"same", mandatory=True)

    unroutable = lookup_sendables(server, m.BasicReturn)[0]

    assert len(lookup_sendables(server, m.BasicReturn)) == 2

    server.send(unroutable)

    assert len(lookup_sendables(server, m.BasicReturn)) == 1
    assert error_replies_to(server, m.BasicPublish)

    server.send(unroutable)

    assert not subsets_any_sendable(server, m.BasicReturn)


def test_identical_returns_with_one_unconfirmed() -> None:
    server = channelled()

    publish(server, body=b"same", mandatory=True)
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    send_unwrap(server, m.QueueDeclareOk)
    publish(server, body=b"same", mandatory=True)

    unroutable = lookup_sendables(server, m.BasicReturn)[0]

    assert len(lookup_sendables(server, m.BasicReturn)) == 2

    server.send(unroutable)

    assert len(lookup_sendables(server, m.BasicReturn)) == 1
    assert error_replies_to(server, m.BasicPublish)

    server.send(unroutable)

    assert not subsets_any_sendable(server, m.BasicReturn)
