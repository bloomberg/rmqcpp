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

# reusing generators from the model
from random import Random

from amqp_imposter.model.generators import GenState, RangedInt
from amqp_imposter.model.method_fields import (
    GEN_BIT,
    GEN_NONEMPTY_NAME,
    GEN_NONEMPTY_SHORTSTR,
    GEN_SHORTSTR,
)

__all__ = [
    "GEN_BIT",
    "GEN_DELIVERIES",
    "GEN_MESSAGE_COUNT",
    "GEN_NONEMPTY_NAME",
    "GEN_NONEMPTY_SHORTSTR",
    "GEN_PREFETCH_COUNT",
    "GEN_SHORTSTR",
    "client_randomness",
    "distinct_names",
]

GEN_PREFETCH_COUNT = RangedInt(1, 30)
GEN_DELIVERIES = RangedInt(1, 50)
GEN_MESSAGE_COUNT = RangedInt(1, 30)


def client_randomness(seed: int) -> Random:
    # advance so the client doesn't draw the same randomness as the imposter
    return Random(GenState(seed).advance().seed)


def distinct_names(randomness: Random, count: int) -> tuple[str, ...]:
    names: list[str] = []
    while len(names) < count:
        name = GEN_NONEMPTY_NAME.from_randomness(randomness)
        if name not in names:
            names.append(name)

    return tuple(names)
