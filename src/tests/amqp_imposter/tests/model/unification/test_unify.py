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
from typing import get_args

from amqp_imposter.model import generators as g
from amqp_imposter.model import method_unification as mu
from amqp_imposter.model import methods as m

from ..helpers import FRESH_DELIVERY


def test_method_kinds_are_distinct() -> None:
    # if this fails, it indicates an issue with our spec for AMQP 0-9-1
    kinds = get_args(m.Method.__value__)
    ids = {(kind.CLASS_ID, kind.METHOD_ID) for kind in kinds}

    assert len(ids) == len(kinds)


def test_unify_myself() -> None:
    assert mu.unify(FRESH_DELIVERY, FRESH_DELIVERY) == FRESH_DELIVERY


def test_unify_myself_with_nones() -> None:
    published = m.BasicPublish(exchange="ex", routing_key="rk", properties=None)

    assert mu.unify(published, published) == published


def test_cannot_unify_kinds() -> None:
    assert mu.unify_any(FRESH_DELIVERY, (m.BasicAck(delivery_tag=1),)) is None


def test_cannot_unify_fields() -> None:
    assert mu.unify(FRESH_DELIVERY, replace(FRESH_DELIVERY, delivery_tag=2)) is None


def test_unify_prefer_given() -> None:
    unified = mu.unify(replace(FRESH_DELIVERY, body=b"chosen"), FRESH_DELIVERY)

    assert unified == replace(FRESH_DELIVERY, body=b"chosen")


def test_unify_prefer_against() -> None:
    unified = mu.unify(FRESH_DELIVERY, replace(FRESH_DELIVERY, body=b"fixed"))

    assert unified == replace(FRESH_DELIVERY, body=b"fixed")


def test_unify_prefer_arbitrary_against() -> None:
    entry = replace(FRESH_DELIVERY, body=g.RandomBytes(1, 1))

    assert mu.unify(FRESH_DELIVERY, entry) == entry


def test_unify_take_both_sides() -> None:
    unified = mu.unify(
        replace(FRESH_DELIVERY, body=b"a"), replace(FRESH_DELIVERY, exchange="ex")
    )

    assert unified == replace(FRESH_DELIVERY, body=b"a", exchange="ex")


def test_unify_any_skip_impossible() -> None:
    entry = replace(FRESH_DELIVERY, body=b"fixed")

    assert mu.unify_any(FRESH_DELIVERY, (m.BasicAck(delivery_tag=1), entry)) == entry


def test_unify_any_greedy() -> None:
    first = replace(FRESH_DELIVERY, body=b"one")
    second = replace(FRESH_DELIVERY, body=b"two")

    assert mu.unify_any(FRESH_DELIVERY, (first, second)) == first


def test_unify_any_impossible() -> None:
    assert (
        mu.unify_any(FRESH_DELIVERY, (replace(FRESH_DELIVERY, delivery_tag=2),)) is None
    )
