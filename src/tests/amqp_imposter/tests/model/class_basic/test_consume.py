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

from amqp_imposter.model import (
    DEFAULT_GEN_STATE,
    Gen,
    Unrepresentable,
)
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import method_unification as mu
from amqp_imposter.model import methods as m

from ..helpers import (
    CONSUMER_TAG,
    OTHER_CONSUMER_TAG,
    OTHER_QUEUE,
    QUEUE,
    QUEUE_ARGUMENTS,
    channel_errors,
    channelled,
    connection_errors,
    consuming,
    error_replies_to,
    lookup_unique_sendable,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)


@pytest.mark.parametrize("no_wait", (False, True), ids=("wait", "no_wait"))
def test_consume_error_options(no_wait: bool) -> None:
    server = channelled()

    receive_unwrap(
        server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG, no_wait=no_wait)
    )

    assert error_replies_to(server, m.BasicConsume) == channel_errors(
        mf.ReplyCode.NOT_FOUND,
        mf.ReplyCode.ACCESS_REFUSED,
        mf.ReplyCode.RESOURCE_LOCKED,
    ) | connection_errors(mf.ReplyCode.RESOURCE_ERROR)


def test_no_consume_ok_if_nowait() -> None:
    server = channelled()

    receive_unwrap(
        server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG, no_wait=True)
    )

    assert not subsets_any_sendable(server, m.BasicConsumeOk)


def test_consume_anonymous_tag() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicConsume(queue=QUEUE, consumer_tag=""))

    assert lookup_unique_sendable(server, m.BasicConsumeOk).consumer_tag


@pytest.mark.xfail(strict=True, reason="anonymous consumer tag is left to future work")
def test_consume_anonymous_tag_nowait() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicConsume(queue=QUEUE, consumer_tag="", no_wait=True))

    assert lookup_unique_sendable(server, m.BasicDeliver).consumer_tag


def test_delivery_has_cluster_id_void() -> None:
    delivery, _ = mu.concretise(
        lookup_unique_sendable(consuming(), m.BasicDeliver), DEFAULT_GEN_STATE
    )

    assert not isinstance(delivery.properties, Gen)
    assert delivery.properties is None or delivery.properties.cluster_id == mf.Void()


def test_nowait_consume_orthogonal_to_delivery() -> None:
    server = channelled()

    receive_unwrap(
        server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG, no_wait=True)
    )

    send_unwrap(server, m.BasicDeliver, consumer_tag=CONSUMER_TAG)


@pytest.mark.parametrize(
    "kind", (m.BasicDeliver, m.BasicCancel), ids=lambda k: k.__name__
)
def test_discard_nowait_consume_error(kind: type[m.Method]) -> None:
    server = channelled()

    receive_unwrap(
        server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG, no_wait=True)
    )
    send_unwrap(server, kind, consumer_tag=CONSUMER_TAG)

    assert not error_replies_to(server, m.BasicConsume)


@pytest.mark.parametrize("declare", (False, True), ids=("adjacent", "declare"))
def test_nowait_reconsume_after_nowait_cancel(declare: bool) -> None:
    server = channelled()

    receive_unwrap(
        server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG, no_wait=True)
    )
    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG, no_wait=True))
    if declare:
        receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))
    receive_unwrap(
        server,
        m.BasicConsume(queue=QUEUE, consumer_tag=OTHER_CONSUMER_TAG, no_wait=True),
    )

    assert not subsets_any_sendable(server, m.BasicDeliver, consumer_tag=CONSUMER_TAG)

    send_unwrap(server, m.BasicDeliver, consumer_tag=OTHER_CONSUMER_TAG)

    assert not error_replies_to(server, m.BasicConsume)
    assert not error_replies_to(server, m.QueueDeclare)


# TODO represent
def test_second_consumer_on_the_same_queue_is_unrepresentable() -> None:
    server = consuming()

    assert isinstance(
        server.receive(m.BasicConsume(queue=QUEUE, consumer_tag=OTHER_CONSUMER_TAG)),
        Unrepresentable,
    )


# TODO represent
def test_second_consumer_on_another_queue_is_unrepresentable() -> None:
    server = consuming()

    assert isinstance(
        server.receive(
            m.BasicConsume(queue=OTHER_QUEUE, consumer_tag=OTHER_CONSUMER_TAG)
        ),
        Unrepresentable,
    )


# TODO represent
def test_reused_consumer_tag_is_unrepresentable() -> None:
    server = consuming()

    assert isinstance(
        server.receive(m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG)),
        Unrepresentable,
    )


# TODO represent
def test_pipelined_second_consumer_is_unrepresentable() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG))

    assert isinstance(
        server.receive(m.BasicConsume(queue=QUEUE, consumer_tag=OTHER_CONSUMER_TAG)),
        Unrepresentable,
    )


def test_consume_with_arguments() -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.BasicConsume(
            queue=QUEUE,
            consumer_tag=CONSUMER_TAG,
            arguments=QUEUE_ARGUMENTS,  # the exact keys are not important
        ),
    )
    send_unwrap(server, m.BasicConsumeOk)

    assert subsets_any_sendable(server, m.BasicDeliver, consumer_tag=CONSUMER_TAG)
