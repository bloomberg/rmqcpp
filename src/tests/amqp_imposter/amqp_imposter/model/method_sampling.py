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

from dataclasses import dataclass
from random import Random

from . import method_unification as mu
from . import method_weights as mw
from . import methods as m
from .generators import Gen

type PatternMethods = tuple[mw.WeightedPattern, tuple[m.Method, ...]]

DEFAULT_MAX_SENDS = 1000


@dataclass(frozen=True, slots=True)
class Stop:
    pass


@dataclass(frozen=True, slots=True)
class MethodSampler(Gen[m.Method | Stop]):
    unified_by_weighted_pattern: tuple[PatternMethods, ...]
    stop_weight: int

    def __post_init__(self) -> None:
        if self.stop_weight <= 0:
            raise mw.BadWeight(f"stop needs a positive weight: {self.stop_weight}")

    def from_randomness(self, randomness: Random) -> m.Method | Stop:
        groups: tuple[tuple[m.Method | Stop, ...], ...] = (
            *(unified for _, unified in self.unified_by_weighted_pattern),
            (Stop(),),
        )
        weights = (
            *(wp.weight for wp, _ in self.unified_by_weighted_pattern),
            self.stop_weight,
        )

        [group] = randomness.choices(groups, weights=weights)
        return randomness.choice(group)


def methods_by_weighted_pattern(
    methods: tuple[m.Method, ...], weighted_patterns: tuple[mw.WeightedPattern, ...]
) -> tuple[PatternMethods, ...]:
    unified_by_pattern: list[PatternMethods] = []

    remaining = methods

    for wp in weighted_patterns:
        # take all the methods we can match by unifying with the pattern
        pairs = tuple((method, mu.unify(wp.pattern, method)) for method in remaining)

        # partition to those that could unify and those that couldn't
        unified = tuple(u for _, u in pairs if u is not None)
        remaining = tuple(method for method, u in pairs if u is None)

        unified_by_pattern.append((wp, unified))

    return tuple(unified_by_pattern)


def make_sampler(
    methods: tuple[m.Method, ...],
    weighted_patterns: tuple[mw.WeightedPattern, ...],
    stop_weight: int,
) -> MethodSampler:
    effective_wps = (
        *weighted_patterns,
        *mw.implicit_weighted_patterns(m.distinct_kinds(methods)),
    )
    return MethodSampler(
        tuple(
            (wp, unified)
            for wp, unified in methods_by_weighted_pattern(methods, effective_wps)
            if wp.weight > 0 and unified
        ),
        stop_weight,
    )
