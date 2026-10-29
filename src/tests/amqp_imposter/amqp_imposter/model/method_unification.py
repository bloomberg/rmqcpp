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
from typing import Any

from . import generators as g
from . import method_fields as mf
from . import methods as m


def unify[M: m.Method](method: M, other: m.Method) -> M | None:
    """
    The unification of two methods, or None if it does not exist.
    If both are arbitrary, the other value is taken.
    """
    if not m.is_same_kind(method, other):
        return None

    unified_fields: dict[str, Any] = {}

    method_fields = m.get_fields(method)
    other_fields = m.get_fields(other)

    for name, value in method_fields.items():
        other_value = other_fields[name]

        if not mf.is_concrete(value):
            unified_fields[name] = other_value
        elif not mf.is_concrete(other_value) or value == other_value:
            unified_fields[name] = value
        else:
            return None

    return m.set_fields(method, unified_fields)


def unify_any[M: m.Method](method: M, others: Iterable[m.Method]) -> M | None:
    """
    The unification with the first other method that unifies, or None if none does.
    """
    for other in others:
        if (unified := unify(method, other)) is not None:
            return unified

    return None


def concretise[M: m.Method](method: M, gen_state: g.GenState) -> tuple[M, g.GenState]:
    """
    Generates missing fields for a method so that is fully specified and ready to encode.
    """
    assert not m.is_method_pattern(method), method

    concrete_values: dict[str, Any] = {}

    for name, value in m.get_fields(method).items():
        gen_state = gen_state.advance()

        match value:
            case g.Gen():
                concrete_values[name] = value.generate(gen_state)[0]
            case _:
                concrete_values[name] = mf.assert_concrete(value)

    return (m.set_fields(method, concrete_values), gen_state)
