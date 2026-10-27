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

from amqp_imposter.model.helpers.revealing_line import (
    Opaque,
    RevealingLine,
    Transparent,
    is_transparent,
)


def test_empty() -> None:
    assert RevealingLine[int]().observe() == ()


def test_push_back() -> None:
    line = RevealingLine[int]().push(Opaque(1)).push(Opaque(2))
    assert tuple(revealed.item for revealed in line) == (1, 2)


def test_observe_until_opaque() -> None:
    line = (
        RevealingLine[int]()
        .push(Transparent(1))
        .push(Transparent(2))
        .push(Opaque(3))
        .push(Transparent(4))
    )
    assert line.observe() == (1, 2, 3)


def test_observe_no_opaque() -> None:
    line = RevealingLine[int]().push(Transparent(1)).push(Transparent(2))
    assert line.observe() == (1, 2)


def test_observe_head_opaque() -> None:
    line = RevealingLine[int]().push(Opaque(1)).push(Opaque(2))
    assert line.observe() == (1,)


def test_split() -> None:
    line = (
        RevealingLine[int]().push(Transparent(1)).push(Opaque(2)).push(Transparent(3))
    )
    front, back = line.split(2)
    assert tuple(revealed.item for revealed in front) == (1, 2)
    assert tuple(revealed.item for revealed in back) == (3,)


def test_every_split() -> None:
    line = RevealingLine[int]()
    line = (
        line.push(Transparent(1))
        .push(Opaque(2))
        .push(Transparent(3))
        .push(Transparent(4))
        .push(Transparent(1))
        .push(Opaque(1))
    )
    for n in range(len(line) + 1):
        front, back = line.split(n)
        assert (*front, *back) == tuple(line)


def test_split_while() -> None:
    line = (
        RevealingLine[int]()
        .push(Transparent(1))
        .push(Transparent(2))
        .push(Opaque(3))
        .push(Transparent(4))
    )
    front, back = line.split_while(is_transparent)
    assert tuple(revealed.item for revealed in front) == (1, 2)
    assert tuple(revealed.item for revealed in back) == (3, 4)
