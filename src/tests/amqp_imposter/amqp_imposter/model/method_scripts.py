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

from . import method_unification as mu
from . import methods as m


@dataclass(frozen=True, slots=True)
class Received:
    method: m.Method


@dataclass(frozen=True, slots=True)
class Sent:
    method: m.Method


@dataclass(frozen=True, slots=True)
class ScriptRule:
    when: Received | Sent
    then_send: m.Method

    def __post_init__(self) -> None:
        m.validate_pattern(self.when.method)
        m.validate_pattern(self.then_send)


def match_rules(
    script: tuple[ScriptRule, ...], when: Received | Sent
) -> tuple[ScriptRule, ...]:
    return tuple(
        rule
        for rule in script
        if type(rule.when) is type(when)
        and mu.unify(rule.when.method, when.method) is not None
    )
