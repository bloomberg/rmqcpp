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

from collections.abc import Iterable
from dataclasses import dataclass

from . import method_fields as mf
from . import methods as m

BASE_WEIGHT = 100
STOP_WEIGHT = BASE_WEIGHT
UNPROMPTED_TEARDOWN_WEIGHT = 0
NOT_FORCED_WEIGHT = 0


class BadWeight(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class WeightedPattern:
    pattern: m.Method
    weight: int = BASE_WEIGHT

    def __post_init__(self) -> None:
        if self.weight < 0:
            raise BadWeight(f"negative weight: {self.weight}")

        m.validate_pattern(self.pattern)


UNPROMPTED_CONNECTION_CLOSE = m.ConnectionClose(
    class_id=mf.UNPROMPTED_CLASS_ID, method_id=mf.UNPROMPTED_METHOD_ID
)
UNPROMPTED_CHANNEL_CLOSE = m.ChannelClose(
    class_id=mf.UNPROMPTED_CLASS_ID, method_id=mf.UNPROMPTED_METHOD_ID
)
UNPROMPTED_CANCEL = m.BasicCancel()
NOT_FORCED_CONNECTION_CLOSE = m.ConnectionClose(reply_text=mf.NOT_FORCED_REPLY_TEXT)
NOT_FORCED_CHANNEL_CLOSE = m.ChannelClose(reply_text=mf.NOT_FORCED_REPLY_TEXT)

DEFAULT_WEIGHTS: tuple[WeightedPattern, ...] = ()


def implicit_weighted_patterns(
    kinds: Iterable[type[m.Method]],
) -> tuple[WeightedPattern, ...]:
    return (
        WeightedPattern(UNPROMPTED_CONNECTION_CLOSE, UNPROMPTED_TEARDOWN_WEIGHT),
        WeightedPattern(UNPROMPTED_CANCEL, UNPROMPTED_TEARDOWN_WEIGHT),
        WeightedPattern(UNPROMPTED_CHANNEL_CLOSE, UNPROMPTED_TEARDOWN_WEIGHT),
        WeightedPattern(NOT_FORCED_CONNECTION_CLOSE, NOT_FORCED_WEIGHT),
        WeightedPattern(NOT_FORCED_CHANNEL_CLOSE, NOT_FORCED_WEIGHT),
        *(WeightedPattern(kind()) for kind in kinds),
    )
