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

from collections.abc import Callable

from ..model import method_fields as mf


def cond_value[T](cond: bool, value: T) -> T | None:
    return value if cond else None


def cond_exact_field[T](cond: bool, value: T) -> mf.Exact[T]:
    return value if cond else mf.HOLE


def cond_map[P, T](cond: bool, value: P, f: Callable[[P], T]) -> T | None:
    return f(value) if cond else None


def concrete_or[T, D = None](
    value: mf.Exact[T] | mf.Arbitrary[T],
    fallback: D = None,  # type: ignore[assignment]
) -> T | D:
    return value if mf.is_concrete(value) else fallback


def concrete_map_or[T, D](
    value: mf.Arbitrary[T | None], f: Callable[[T], D]
) -> D | None:
    if (concrete := concrete_or(value, None)) is not None:
        return f(concrete)
    else:
        return None
