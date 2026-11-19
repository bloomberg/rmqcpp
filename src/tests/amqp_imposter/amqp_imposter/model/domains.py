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

from decimal import Decimal, InvalidOperation
from typing import ClassVar, Self


class BadDomain(ValueError):
    pass


class BoundedInt(int):
    __slots__ = ()

    MIN: ClassVar[int] = 0
    MAX: ClassVar[int]

    def __new__(cls, value: int) -> Self:
        if not cls.MIN <= value <= cls.MAX:
            raise BadDomain(f"{cls.__name__} outside [{cls.MIN}, {cls.MAX}]: {value}")

        return super().__new__(cls, value)


class Octet(BoundedInt):
    __slots__ = ()

    MAX = 2**8 - 1


class Short(BoundedInt):
    __slots__ = ()

    MAX = 2**16 - 1


class Long(BoundedInt):
    __slots__ = ()

    MAX = 2**32 - 1


class LongLong(BoundedInt):
    __slots__ = ()

    MAX = 2**64 - 1


class SignedLongLong(BoundedInt):
    __slots__ = ()

    MIN = -(2**63)
    MAX = 2**63 - 1


class Timestamp(BoundedInt):
    __slots__ = ()

    # to match python datetime
    MAX = 253_402_300_799


class ShortStr(str):
    __slots__ = ()

    MAX_OCTETS: ClassVar[int] = 2**8 - 1

    def __new__(cls, value: str) -> Self:
        if len(value.encode()) > cls.MAX_OCTETS:
            raise BadDomain(f"{cls.__name__} over {cls.MAX_OCTETS} octets: {value!r}")

        return super().__new__(cls, value)


class NonEmptyShortStr(ShortStr):
    __slots__ = ()

    def __new__(cls, value: str) -> Self:
        if not value:
            raise BadDomain(f"{cls.__name__} empty")

        return super().__new__(cls, value)


class DecimalValue(Decimal):
    __slots__ = ()

    # a scale octet plus a value
    MAX_SCALE: ClassVar[int] = 2**8 - 1

    MIN_UNSCALED: ClassVar[int] = -(2**31)
    MAX_UNSCALED: ClassVar[int] = 2**31 - 1

    def __new__(cls, value: Decimal | str | int) -> Self:
        try:
            decimal = super().__new__(cls, value)
        except InvalidOperation as bad:
            raise BadDomain(f"{cls.__name__} unparsable: {value!r}") from bad

        match decimal.normalize().as_tuple().exponent:
            case int() as exponent:
                scale = max(0, -exponent)
            case _:
                raise BadDomain(f"{cls.__name__} not finite: {value!r}")

        unscaled = int(decimal.scaleb(scale))

        if scale > cls.MAX_SCALE or not (
            cls.MIN_UNSCALED <= unscaled <= cls.MAX_UNSCALED
        ):
            raise BadDomain(f"{cls.__name__} not encodable: {value!r}")

        return decimal
