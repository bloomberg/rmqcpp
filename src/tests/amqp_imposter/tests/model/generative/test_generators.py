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

from amqp_imposter.model import GenState
from amqp_imposter.model import generators as g

STATE = GenState(12345)
SAMPLES = 200


def test_comparable() -> None:
    assert g.RandomBytes(0, 64) == g.RandomBytes(0, 64)
    assert g.RandomBytes(0, 64) != g.RandomBytes(0, 32)
    assert g.RandomText(1, 4) != g.RandomText(1, 8)


def test_hashable() -> None:
    assert len({g.RandomBytes(0, 64), g.RandomBytes(0, 64), g.RandomBytes(0, 32)}) == 2


def test_pure() -> None:
    assert g.RandomBytes(1, 8).generate(STATE) == g.RandomBytes(1, 8).generate(STATE)


def test_advance() -> None:
    _, gen_state = g.RangedInt(1, 2).generate(STATE)

    assert gen_state != STATE


def test_constant() -> None:
    assert g.Constant(b"k").generate(STATE) == (b"k", STATE)


def test_advance_diverges() -> None:
    states = [STATE]
    for _ in range(SAMPLES - 1):
        states.append(states[-1].advance())

    assert len(set(states)) == SAMPLES


def test_int_within_range() -> None:
    drawn = {g.RangedInt(3, 7).generate(GenState(seed))[0] for seed in range(SAMPLES)}

    assert drawn <= set(range(3, 8))
    assert len(drawn) > 1


def test_bytes_within_range() -> None:
    sizes = {
        len(g.RandomBytes(2, 5).generate(GenState(seed))[0]) for seed in range(SAMPLES)
    }

    assert sizes <= set(range(2, 6))
    assert len(sizes) > 1


def test_random_text_within_length_bounds() -> None:
    lengths = {
        len(g.RandomText(2, 5).generate(GenState(seed))[0]) for seed in range(SAMPLES)
    }

    assert lengths <= set(range(2, 6))
    assert len(lengths) > 1


def test_even_chance_gives_both() -> None:
    assert {
        g.NonzeroChance(0.5).generate(GenState(seed))[0] for seed in range(SAMPLES)
    } == {True, False}


def test_certain_chance() -> None:
    assert all(
        g.NonzeroChance(1).generate(GenState(seed))[0] for seed in range(SAMPLES)
    )


def test_bad_bounds() -> None:
    with pytest.raises(g.BadBounds):
        g.RangedInt(5, 1)
    with pytest.raises(g.BadBounds):
        g.NonzeroChance(0)
    with pytest.raises(g.BadBounds):
        g.NonzeroChance(1.5)
    with pytest.raises(g.BadBounds):
        g.RandomBytes(5, 1)
    with pytest.raises(g.BadBounds):
        g.RandomText(-1, 1)
