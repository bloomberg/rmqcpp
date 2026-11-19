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

from decimal import Decimal

import pytest

from amqp_imposter.model import domains as d


@pytest.mark.parametrize(
    "domain",
    (d.Octet, d.Short, d.Long, d.LongLong, d.SignedLongLong, d.Timestamp),
    ids=lambda one: one.__name__,
)
def test_int_bounds(domain: type[d.BoundedInt]) -> None:
    assert domain(domain.MIN) == domain.MIN
    assert domain(domain.MAX) == domain.MAX

    for bad in (domain.MIN - 1, domain.MAX + 1):
        with pytest.raises(d.BadDomain):
            domain(bad)


@pytest.mark.parametrize(
    "domain", (d.ShortStr, d.NonEmptyShortStr), ids=lambda one: one.__name__
)
def test_shortstr_bounds_octets(domain: type[d.ShortStr]) -> None:
    longest = "a" * domain.MAX_OCTETS

    assert domain(longest) == longest

    with pytest.raises(d.BadDomain):
        domain(longest + "a")
    with pytest.raises(d.BadDomain):
        domain("é" * (domain.MAX_OCTETS // 2 + 1))


def test_nonempty_shortstr() -> None:
    assert d.NonEmptyShortStr("a") == "a"
    assert d.ShortStr("") == ""

    with pytest.raises(d.BadDomain):
        d.NonEmptyShortStr("")


def test_decimal_bounds() -> None:
    for ok in ("1.25", "-0.001", "2147483647", "-2147483648", "1E+9", "1E-255"):
        assert d.DecimalValue(ok) == Decimal(ok)

    for bad in ("abc", "NaN", "Infinity", "2147483648", "1E+10", "1E-256"):
        with pytest.raises(d.BadDomain):
            d.DecimalValue(bad)
