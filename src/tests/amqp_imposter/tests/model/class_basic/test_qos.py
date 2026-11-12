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

from amqp_imposter.model import Unrepresentable
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    channelled,
    connection_errors,
    consume,
    consuming,
    consuming_no_qos,
    consuming_zero_prefetch_count,
    deliver,
    error_replies_to,
    prefetch_count_filled,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
)

HIGH_PREFETCH = 1_000


def test_qos_ok_prompts_nothing() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicQos(prefetch_count=1))
    send_unwrap(server, m.BasicQosOk)

    assert not subsets_any_sendable(server, m.BasicQosOk)
    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_global_qos_is_unrepresentable() -> None:
    server = channelled()

    assert isinstance(server.receive(m.BasicQos(global_qos=True)), Unrepresentable)


def test_global_qos_precedes_prefetch_size() -> None:
    server = channelled()

    assert isinstance(
        server.receive(m.BasicQos(prefetch_size=1, global_qos=True)), Unrepresentable
    )


def test_channel_qos_bounds_deliveries() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicQos(prefetch_count=1, global_qos=False))
    send_unwrap(server, m.BasicQosOk)
    consume(server)
    deliver(server, 1)

    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_prefetch_size_is_not_implemented() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicQos(prefetch_size=1))

    assert error_replies_to(server, m.BasicQos) == connection_errors(
        mf.ReplyCode.NOT_IMPLEMENTED
    )


def test_zero_prefetch_never_fills() -> None:
    server = consuming_zero_prefetch_count()

    for delivery_tag in range(1, HIGH_PREFETCH):
        deliver(server, delivery_tag)

    assert subsets_any_sendable(server, m.BasicDeliver)


def test_absent_qos_never_fills() -> None:
    server = consuming_no_qos()

    for delivery_tag in range(1, HIGH_PREFETCH):
        deliver(server, delivery_tag)

    assert subsets_any_sendable(server, m.BasicDeliver)


def test_qos_only_after_consume() -> None:
    server = consuming_no_qos()

    deliver(server, 1)
    receive_unwrap(server, m.BasicQos(prefetch_count=1))

    assert subsets_any_sendable(server, m.BasicDeliver)

    send_unwrap(server, m.BasicQosOk)

    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_zero_prefetch_unblocks_a_full_channel() -> None:
    server = prefetch_count_filled(1)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicQos(prefetch_count=0))
    send_unwrap(server, m.BasicQosOk)

    assert subsets_any_sendable(server, m.BasicDeliver)


def test_prefetch_after_zero_blocks() -> None:
    server = consuming_zero_prefetch_count()

    deliver(server, 1)
    receive_unwrap(server, m.BasicQos(prefetch_count=1))
    send_unwrap(server, m.BasicQosOk)

    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_repeat_qos_raises_the_bound() -> None:
    server = consuming(prefetch_count=1)

    deliver(server, 1)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicQos(prefetch_count=2))
    send_unwrap(server, m.BasicQosOk)

    deliver(server, 2)

    assert not subsets_any_sendable(server, m.BasicDeliver)
