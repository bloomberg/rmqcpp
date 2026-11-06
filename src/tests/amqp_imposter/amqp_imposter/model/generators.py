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

from abc import ABC, abstractmethod
from collections.abc import Callable
from dataclasses import dataclass
from random import Random

from ..helpers.nonempty_tuple import NonEmptyTuple

SEED_BITS = 64


class BadBounds(ValueError):
    pass


@dataclass(frozen=True, slots=True)
class GenState:
    seed: int

    def advance(self) -> GenState:
        seed_randomness = Random(self.seed)
        return GenState(seed_randomness.getrandbits(SEED_BITS))

    def generate[T](self, from_randomness: Callable[[Random], T]) -> tuple[T, GenState]:
        randomness = Random(self.seed)
        return from_randomness(randomness), GenState(randomness.getrandbits(SEED_BITS))


DEFAULT_GEN_STATE = GenState(0)


class Gen[T](ABC):
    __slots__ = ()

    @abstractmethod
    def from_randomness(self, randomness: Random) -> T: ...

    def generate(self, gen_state: GenState) -> tuple[T, GenState]:
        return gen_state.generate(self.from_randomness)


@dataclass(frozen=True, slots=True)
class Constant[T](Gen[T]):
    value: T

    def from_randomness(self, randomness: Random) -> T:
        return self.value

    def generate(self, gen_state: GenState) -> tuple[T, GenState]:
        return self.value, gen_state


@dataclass(frozen=True, slots=True)
class Choice[T](Gen[T]):
    choices: NonEmptyTuple[T]

    def from_randomness(self, randomness: Random) -> T:
        return randomness.choice(self.choices)


@dataclass(frozen=True, slots=True)
class RangedInt(Gen[int]):
    min_value: int
    max_value: int

    def __post_init__(self) -> None:
        if self.min_value > self.max_value:
            raise BadBounds(f"bad range: [{self.min_value}, {self.max_value}]")

    def from_randomness(self, randomness: Random) -> int:
        return randomness.randint(self.min_value, self.max_value)


@dataclass(frozen=True, slots=True)
class NonzeroChance(Gen[bool]):
    chance: float

    def __post_init__(self) -> None:
        if not 0 < self.chance <= 1:
            raise BadBounds(f"chance outside (0, 1]: {self.chance}")

    def from_randomness(self, randomness: Random) -> bool:
        return randomness.random() < self.chance


@dataclass(frozen=True, slots=True)
class RandomBytes(Gen[bytes]):
    min_length: int
    max_length: int

    def __post_init__(self) -> None:
        if self.min_length < 0 or self.min_length > self.max_length:
            raise BadBounds(f"bad range: [{self.min_length}, {self.max_length}]")

    def from_randomness(self, randomness: Random) -> bytes:
        return randomness.randbytes(
            randomness.randint(self.min_length, self.max_length)
        )


@dataclass(frozen=True, slots=True)
class RandomText(Gen[str]):
    min_length: int
    max_length: int

    def __post_init__(self) -> None:
        if self.min_length < 0 or self.min_length > self.max_length:
            raise BadBounds(f"bad range: [{self.min_length}, {self.max_length}]")

    def from_randomness(self, randomness: Random) -> str:
        length = randomness.randint(self.min_length, self.max_length)
        blob = randomness.randbytes(length).hex()
        return blob[:length]
