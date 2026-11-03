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

from amqp_imposter.model import ModelServer
from amqp_imposter.model import methods as m

from ..helpers import (
    confirming,
    delivery_tags,
    lookup_sendables,
    publish,
    send_unwrap,
)

UNCONFIRMED = 1_000


def body(delivery_tag: int) -> bytes:
    return f"m-{delivery_tag}".encode()


def confirmable(first: int, last: int) -> set[tuple[int, bool]]:
    return {
        (delivery_tag, multiple)
        for delivery_tag in range(first, last + 1)
        for multiple in (False, True)
    }


def unconfirmed(count: int, mandatory: bool = False) -> ModelServer:
    server = confirming()
    for delivery_tag in range(1, count + 1):
        publish(server, body=body(delivery_tag), mandatory=mandatory)
    return server


def test_many_unconfirmed_in_bag() -> None:
    server = unconfirmed(UNCONFIRMED)

    assert delivery_tags(server, m.BasicAck) == confirmable(1, UNCONFIRMED)
    assert delivery_tags(server, m.BasicNack) == confirmable(1, UNCONFIRMED)

    send_unwrap(server, m.BasicAck, delivery_tag=UNCONFIRMED, multiple=True)

    assert delivery_tags(server, m.BasicAck) == set()
    assert delivery_tags(server, m.BasicNack) == set()


def test_number_of_confirms_in_bag() -> None:
    server = unconfirmed(UNCONFIRMED)

    # let's count the acks:
    # 1 for each unconfirmed tag alone
    # 1 for multiple ack up to each unconfirmed tag
    # N.b. that Ack(1, True) and Ack(1, False) are different frames even if they have the same effect!
    assert len(lookup_sendables(server, m.BasicAck)) == 2 * UNCONFIRMED
    assert len(lookup_sendables(server, m.BasicNack)) == 2 * UNCONFIRMED


def test_multiple_ack() -> None:
    server = unconfirmed(UNCONFIRMED)

    send_unwrap(server, m.BasicAck, delivery_tag=UNCONFIRMED // 2, multiple=True)

    assert delivery_tags(server, m.BasicAck) == confirmable(
        UNCONFIRMED // 2 + 1, UNCONFIRMED
    )
    assert delivery_tags(server, m.BasicNack) == confirmable(
        UNCONFIRMED // 2 + 1, UNCONFIRMED
    )


def test_unordered_single_acks() -> None:
    server = unconfirmed(UNCONFIRMED)

    for delivery_tag in range(UNCONFIRMED, UNCONFIRMED // 2, -1):
        server.send(m.BasicAck(delivery_tag=delivery_tag))

    assert delivery_tags(server, m.BasicAck) == confirmable(1, UNCONFIRMED // 2)

    for delivery_tag in range(UNCONFIRMED // 2, 0, -1):
        server.send(m.BasicAck(delivery_tag=delivery_tag))

    assert delivery_tags(server, m.BasicAck) == set()
    assert delivery_tags(server, m.BasicNack) == set()


def test_next_delivery_tags_after_multiple_ack() -> None:
    server = unconfirmed(UNCONFIRMED)

    send_unwrap(server, m.BasicNack, delivery_tag=UNCONFIRMED, multiple=True)
    publish(server)

    assert delivery_tags(server, m.BasicAck) == confirmable(
        UNCONFIRMED + 1, UNCONFIRMED + 1
    )


def test_publish_confirm_repeat() -> None:
    server = confirming()

    for delivery_tag in range(1, UNCONFIRMED + 1):
        publish(server)

        assert delivery_tags(server, m.BasicAck) == confirmable(
            delivery_tag, delivery_tag
        )

        server.send(m.BasicAck(delivery_tag=delivery_tag))

    assert delivery_tags(server, m.BasicAck) == set()


def test_many_returns_in_bag() -> None:
    server = unconfirmed(UNCONFIRMED, mandatory=True)

    returns = lookup_sendables(server, m.BasicReturn)

    assert len(returns) == UNCONFIRMED
    assert {returned.body for returned in returns} == {
        body(delivery_tag) for delivery_tag in range(1, UNCONFIRMED + 1)
    }
    assert delivery_tags(server, m.BasicNack) == confirmable(1, UNCONFIRMED)
