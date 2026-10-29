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

from amqp_imposter.model import DEFAULT_GEN_STATE, Gen
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import method_unification as mu
from amqp_imposter.model import methods as m

from ..helpers import (
    DO_CHANNEL_CLOSE,
    DO_CONNECTION_CLOSE,
    DoTeardown,
    channel_errors,
    channelled,
    close_channel_from_client,
    consume,
    consuming,
    deliver,
    enable_channel_error,
    error_replies_to,
    get,
    lookup_sendables,
    lookup_unique_sendable,
    match_fresh_get_ok,
    polling,
    receive_unwrap,
    reconnect,
    reopen_channel,
    send_get_ok,
    send_unwrap,
    subsets_any_sendable,
)


def test_get_ok() -> None:
    server = polling()

    assert subsets_any_sendable(server, m.BasicGetEmpty)

    got = lookup_unique_sendable(server, m.BasicGetOk)

    assert got.delivery_tag == 1

    server.send(got)

    assert not subsets_any_sendable(server, m.BasicGetOk)
    assert not subsets_any_sendable(server, m.BasicGetEmpty)

    receive_unwrap(server, m.BasicAck(delivery_tag=1))

    assert not subsets_any_sendable(server, m.ChannelClose)


def test_get_error_options() -> None:
    assert error_replies_to(polling(), m.BasicGet) == channel_errors(
        mf.ReplyCode.NOT_FOUND,
        mf.ReplyCode.ACCESS_REFUSED,
        mf.ReplyCode.RESOURCE_LOCKED,
    )


def test_get_empty_spends_no_delivery_tag() -> None:
    server = polling()

    send_unwrap(server, m.BasicGetEmpty)

    assert not subsets_any_sendable(server, m.BasicGetOk)
    assert not subsets_any_sendable(server, m.BasicGetEmpty)

    get(server)

    assert lookup_unique_sendable(server, m.BasicGetOk).delivery_tag == 1


def test_no_get_reply_unprompted() -> None:
    server = channelled()

    assert not subsets_any_sendable(server, m.BasicGetOk)
    assert not subsets_any_sendable(server, m.BasicGetEmpty)


def test_no_requeued_get_reply_unprompted() -> None:
    server = polling()

    send_get_ok(server, body=b"blabla")
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))

    assert not subsets_any_sendable(server, m.BasicGetOk)
    assert not subsets_any_sendable(server, m.BasicGetEmpty)

    get(server)

    assert subsets_any_sendable(server, m.BasicGetOk, redelivered=True)


def test_get_next_delivery_tag() -> None:
    server = polling()

    send_unwrap(server, m.BasicGetOk)
    get(server)

    assert lookup_unique_sendable(server, m.BasicGetOk).delivery_tag == 2


def test_get_next_delivery_tag_simultaneously_consuming() -> None:
    server = consuming(prefetch_count=2)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    get(server)

    assert lookup_unique_sendable(server, m.BasicGetOk).delivery_tag == 2


def test_get_ok_has_cluster_id_void() -> None:
    got, _ = mu.concretise(
        lookup_unique_sendable(polling(), m.BasicGetOk), DEFAULT_GEN_STATE
    )

    assert not isinstance(got.properties, Gen)
    assert got.properties is None or got.properties.cluster_id == mf.Void()


def test_get_full_prefetch() -> None:
    # get fills prefetch, but the prefetch count only stops consumption, not polling
    server = consuming(prefetch_count=1)

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    get(server)

    assert subsets_any_sendable(server, m.BasicGetOk)


def test_pipelined_gets_take_successive_delivery_tags() -> None:
    server = polling()

    get(server)

    assert send_unwrap(server, m.BasicGetOk).delivery_tag == 1
    assert lookup_unique_sendable(server, m.BasicGetOk).delivery_tag == 2


def test_pipelined_get_after_empty_spends_no_delivery_tag() -> None:
    server = polling()

    get(server)
    send_unwrap(server, m.BasicGetEmpty)

    assert lookup_unique_sendable(server, m.BasicGetOk).delivery_tag == 1


def test_only_the_first_pipelined_get_is_answerable() -> None:
    server = polling()

    get(server)

    assert len(lookup_sendables(server, m.BasicGetEmpty)) == 1


def test_fresh_get_has_free_body() -> None:
    assert isinstance(match_fresh_get_ok(polling()).body, Gen)


def test_get_returns_requeued_message() -> None:
    server = polling()

    send_get_ok(server, body=b"blabla")
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))
    get(server)

    again = lookup_unique_sendable(server, m.BasicGetOk, redelivered=True)

    assert again.body == b"blabla"
    assert again.delivery_tag == 2


def test_get_retires_requeued_message() -> None:
    server = polling()

    send_get_ok(server, body=b"blabla")
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))
    get(server)
    send_unwrap(server, m.BasicGetOk, redelivered=True)
    get(server)

    assert not subsets_any_sendable(server, m.BasicGetOk, redelivered=True)


def test_get_leaves_other_requeued_messages() -> None:
    server = polling()

    send_get_ok(server, body=b"one")
    get(server)
    send_get_ok(server, body=b"two")
    receive_unwrap(server, m.BasicNack(delivery_tag=2, multiple=True, requeue=True))
    get(server)
    send_unwrap(server, m.BasicGetOk, body=b"one")
    get(server)

    assert lookup_unique_sendable(server, m.BasicGetOk, redelivered=True).body == b"two"


def test_consumer_redelivers_message_requeued_from_get() -> None:
    server = polling()

    send_get_ok(server, body=b"blabla")
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))
    consume(server)

    assert lookup_unique_sendable(server, m.BasicDeliver, redelivered=True).body == (
        b"blabla"
    )


def test_get_redelivers_message_requeued_from_consumer() -> None:
    server = consuming()

    delivered = deliver(server, 1)
    receive_unwrap(server, m.BasicReject(delivery_tag=1, requeue=True))
    get(server)

    again = lookup_unique_sendable(server, m.BasicGetOk, redelivered=True)

    assert again.body == delivered.body


def test_get_fills_prefetch() -> None:
    # this is unclear from the spec, so this test is an interpretative decision
    # TODO: if we support multiple consumers on one channel, or global qos, revisit this
    server = consuming(prefetch_count=1)

    get(server)
    send_unwrap(server, m.BasicGetOk)

    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_get_ack_frees_prefetch() -> None:
    server = consuming(prefetch_count=1)

    get(server)
    got = send_unwrap(server, m.BasicGetOk)
    receive_unwrap(server, m.BasicAck(delivery_tag=got.delivery_tag))

    assert subsets_any_sendable(server, m.BasicDeliver)


def test_no_ack_get_leaves_prefetch() -> None:
    server = consuming(prefetch_count=1)

    get(server, no_ack=True)
    send_unwrap(server, m.BasicGetOk)

    assert subsets_any_sendable(server, m.BasicDeliver)


def test_multiple_ack_spans_get_and_delivery() -> None:
    server = consuming(prefetch_count=3)

    deliver(server, 1)
    get(server)
    send_unwrap(server, m.BasicGetOk, delivery_tag=2)
    deliver(server, 3)
    receive_unwrap(server, m.BasicAck(delivery_tag=3, multiple=True))

    assert not subsets_any_sendable(server, m.ChannelClose)
    assert not subsets_any_sendable(server, m.BasicDeliver, redelivered=True)


def test_multiple_nack_requeues_get_and_delivery() -> None:
    server = consuming(prefetch_count=3)

    deliver(server, 1)
    get(server)
    send_unwrap(server, m.BasicGetOk, delivery_tag=2)
    deliver(server, 3)
    receive_unwrap(server, m.BasicNack(delivery_tag=3, multiple=True, requeue=True))

    assert len(lookup_sendables(server, m.BasicDeliver, redelivered=True)) == 3


def test_pending_get_precedes_channel_error() -> None:
    server = polling()

    enable_channel_error(server)

    assert not error_replies_to(server, m.BasicAck)

    send_unwrap(server, m.BasicGetEmpty)

    assert error_replies_to(server, m.BasicAck)


def test_channel_close_discards_pending_get() -> None:
    server = polling()

    close_channel_from_client(server)
    reopen_channel(server)

    assert not subsets_any_sendable(server, m.BasicGetOk)
    assert not subsets_any_sendable(server, m.BasicGetEmpty)


@pytest.mark.parametrize("close", DO_CHANNEL_CLOSE, ids=lambda c: c.__name__)
def test_channel_close_requeues_polled_message(close: DoTeardown) -> None:
    server = polling()

    send_get_ok(server, body=b"blabla")
    close(server)
    reopen_channel(server)
    consume(server)

    assert lookup_unique_sendable(server, m.BasicDeliver, redelivered=True).body == (
        b"blabla"
    )


@pytest.mark.parametrize("close", DO_CONNECTION_CLOSE, ids=lambda c: c.__name__)
def test_connection_close_requeues_polled_message(close: DoTeardown) -> None:
    server = polling()

    send_get_ok(server, body=b"blabla")
    close(server)
    reconnect(server)
    consume(server)

    assert lookup_unique_sendable(server, m.BasicDeliver, redelivered=True).body == (
        b"blabla"
    )
