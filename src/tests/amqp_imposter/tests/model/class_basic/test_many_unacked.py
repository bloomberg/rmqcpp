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
    consuming,
    deliver,
    lookup_sendables,
    match_fresh_delivery,
    prefetch_count_filled,
    receive_unwrap,
    subsets_any_sendable,
)

PREFETCH_COUNT = 1000
REQUEUED = 100


def test_many_unacked_in_bag() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicAck(delivery_tag=PREFETCH_COUNT, multiple=True))

    assert match_fresh_delivery(server).delivery_tag == PREFETCH_COUNT + 1


def test_at_most_one_fresh_delivery_in_bag() -> None:
    # only one fresh delivery is ever on offer
    server = consuming(prefetch_count=PREFETCH_COUNT)

    for delivery_tag in range(1, PREFETCH_COUNT + 1):
        assert len(lookup_sendables(server, m.BasicDeliver)) == 1
        deliver(server, delivery_tag)

    # prefetch is full now
    assert not subsets_any_sendable(server, m.BasicDeliver)


def test_many_requeue_deliveries_in_bag() -> None:
    server = consuming(prefetch_count=PREFETCH_COUNT)

    for delivery_tag in range(1, REQUEUED + 1):
        deliver(server, delivery_tag)

    for delivery_tag in range(1, REQUEUED + 1):
        receive_unwrap(server, m.BasicNack(delivery_tag=delivery_tag, requeue=True))

    assert len(lookup_sendables(server, m.BasicDeliver, redelivered=True)) == REQUEUED


def test_partial_ack() -> None:
    server = prefetch_count_filled(PREFETCH_COUNT)

    receive_unwrap(server, m.BasicAck(delivery_tag=500, multiple=True))

    for delivery_tag in range(PREFETCH_COUNT + 1, PREFETCH_COUNT + 501):
        deliver(server, delivery_tag)

    assert not subsets_any_sendable(server, m.BasicDeliver)
