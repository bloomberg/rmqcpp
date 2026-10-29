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

from dataclasses import replace

from amqp_imposter.model import Unrepresentable
from amqp_imposter.model import methods as m

from ..helpers import (
    QUEUE,
    channelled,
    confirming,
    consuming,
    lookup_sendables,
    lookup_unique_sendable,
    match_fresh_delivery,
    publish,
    receive_unwrap,
    send_unwrap,
)


def test_send_removes_one_match() -> None:
    server = channelled()

    publish(server, body=b"same", mandatory=True)
    publish(server, body=b"same", mandatory=True)

    unroutable = lookup_sendables(server, m.BasicReturn)[0]
    server.send(unroutable)

    assert len(lookup_sendables(server, m.BasicReturn)) == 1


def test_send_bad_field_match() -> None:
    server = confirming()

    publish(server)

    assert isinstance(
        server.send(m.BasicAck(delivery_tag=99, multiple=False)), Unrepresentable
    )


def test_send_bad_content_match() -> None:
    server = channelled()
    other = channelled()

    publish(server, body=b"limestone", mandatory=True)
    publish(other, body=b"granite", mandatory=True)

    assert isinstance(
        server.send(lookup_unique_sendable(other, m.BasicReturn)), Unrepresentable
    )


def test_illegal_send_does_not_advance() -> None:
    server = consuming(prefetch_count=1)
    before_sendable = server.sendables()
    before_gen = server.gen_state

    assert isinstance(server.send(m.BasicDeliver("nobody", 99)), Unrepresentable)

    assert server.sendables() == before_sendable
    # critical for anything generative to be deterministic!
    assert server.gen_state == before_gen


def test_fully_specified_send_splits_gen() -> None:
    server = channelled()
    receive_unwrap(server, m.ConfirmSelect())
    before = server.gen_state

    send_unwrap(server, m.ConfirmSelectOk)

    assert server.gen_state != before


def test_deliver_uses_overriden_body() -> None:
    server = consuming(prefetch_count=1)

    fresh = match_fresh_delivery(server)
    server.send(replace(fresh, body=b"blabla"))
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))

    again = lookup_unique_sendable(server, m.BasicDeliver, redelivered=True)

    assert again.body == b"blabla"


def test_old_redelivery_still_available() -> None:
    server = consuming(prefetch_count=2)

    fresh = match_fresh_delivery(server)
    server.send(replace(fresh, body=b"blabla"))
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))

    again = lookup_unique_sendable(server, m.BasicDeliver, redelivered=True)
    server.send(replace(again, body=b"something else"))

    still_here = lookup_unique_sendable(server, m.BasicDeliver, redelivered=True)

    assert still_here.body == b"blabla"


def test_get_ok_uses_overriden_body() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicGet(queue=QUEUE))
    got = lookup_unique_sendable(server, m.BasicGetOk)

    sent = server.send(replace(got, body=b"blabla"))

    assert isinstance(sent, m.BasicGetOk)
    assert sent.body == b"blabla"
