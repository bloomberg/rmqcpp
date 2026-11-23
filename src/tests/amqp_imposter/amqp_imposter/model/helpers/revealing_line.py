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

from collections.abc import Callable, Iterator
from dataclasses import dataclass
from typing import TypeIs


@dataclass(frozen=True, slots=True)
class Transparent[T]:
    item: T


@dataclass(frozen=True, slots=True)
class Opaque[T]:
    item: T


type Revealed[T] = Transparent[T] | Opaque[T]


def is_transparent[T](revealed: Revealed[T]) -> TypeIs[Transparent[T]]:
    return isinstance(revealed, Transparent)


def is_opaque[T](revealed: Revealed[T]) -> TypeIs[Opaque[T]]:
    return isinstance(revealed, Opaque)


@dataclass(frozen=True, slots=True)
class RevealingLine[T]:
    line: tuple[Revealed[T], ...] = ()

    def push(self, revealed: Revealed[T]) -> RevealingLine[T]:
        return RevealingLine((*self.line, revealed))

    def split(self, n: int | None = None) -> tuple[RevealingLine[T], RevealingLine[T]]:
        if n is not None:
            before = self.line[:n]
            after = self.line[n:]
            return RevealingLine(before), RevealingLine(after)
        else:
            before_opaque, rest = self.split_while(is_transparent)
            opaque, after_opaque = rest.split(1)
            return RevealingLine((*before_opaque, *opaque)), after_opaque

    def split_while(
        self, predicate: Callable[[Revealed[T]], bool]
    ) -> tuple[RevealingLine[T], RevealingLine[T]]:
        before: list[Revealed[T]] = []
        after: list[Revealed[T]] = []

        splitting = True
        for item in self.line:
            if splitting and predicate(item):
                before.append(item)
            else:
                splitting = False
                after.append(item)

        return RevealingLine(tuple(before)), RevealingLine(tuple(after))

    def last(self) -> Revealed[T] | None:
        return self.line[-1] if self.line else None

    def items(self) -> tuple[T, ...]:
        return tuple(revealed.item for revealed in self.line)

    def observe(self) -> tuple[T, ...]:
        observable, _ = self.split()
        return observable.items()

    def __iter__(self) -> Iterator[Revealed[T]]:
        return iter(self.line)

    def __len__(self) -> int:
        return len(self.line)
