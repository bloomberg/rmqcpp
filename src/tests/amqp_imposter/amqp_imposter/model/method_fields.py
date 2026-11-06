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
from datetime import datetime
from decimal import Decimal
from enum import IntEnum
from random import Random
from typing import Any, ClassVar, TypeIs

from amqp_imposter.model.constants import (
    DEFAULT_CONTENT_ENCODINGS,
    DEFAULT_CONTENT_TYPES,
    DEFAULT_EXCHANGE_TYPES,
    DEFAULT_OVERFLOWS,
    DEFAULT_QUEUE_TYPES,
    DEFAULT_VHOST,
    DELIVERY_MODES,
    LONG_MAX,
    MESSAGE_BODY_MAX,
    NAME_MAX,
    OCTET_MAX,
    PRIORITY_MAX,
    PRIORITY_MIN,
    SHORT_MAX,
    SHORTSTR_MAX,
)

from .generators import (
    Choice,
    Constant,
    Gen,
    NonzeroChance,
    RandomBytes,
    RandomText,
    RangedInt,
)


@dataclass(frozen=True, slots=True)
class Hole:
    pass


HOLE = Hole()

type Exact[T] = T | Hole
type Arbitrary[T] = T | Gen[T]


def is_concrete[T](value: Exact[T] | Arbitrary[T]) -> TypeIs[T]:
    return not isinstance(value, Gen | Hole)


def assert_concrete[T](value: Exact[T] | Arbitrary[T]) -> T:
    assert is_concrete(value), value
    return value


@dataclass(frozen=True, slots=True)
class Void:
    pass  # this becomes b'V' on the wire, different to the field being absent


type TableValue = (
    bool
    | int
    | str
    | bytes
    | Decimal
    | datetime
    | Void
    | Table
    | tuple[TableValue, ...]
)
Table = frozendict[str, TableValue]


class ClassId(IntEnum):
    CONNECTION = 10
    CHANNEL = 20
    EXCHANGE = 40
    QUEUE = 50
    BASIC = 60
    CONFIRM = 85


class ReplyCode(IntEnum):
    SUCCESS = 200
    CONTENT_TOO_LARGE = 311
    UNROUTABLE = 312
    NO_CONSUMERS = 313
    CONNECTION_FORCED = 320
    INVALID_PATH = 402
    ACCESS_REFUSED = 403
    NOT_FOUND = 404
    RESOURCE_LOCKED = 405
    PRECONDITION_FAILED = 406
    FRAME_ERROR = 501
    SYNTAX_ERROR = 502
    COMMAND_INVALID = 503
    CHANNEL_ERROR = 504
    UNEXPECTED_FRAME = 505
    RESOURCE_ERROR = 506
    NOT_ALLOWED = 530
    NOT_IMPLEMENTED = 540
    INTERNAL_ERROR = 541


SOFT_ERRORS = frozenset(
    {
        ReplyCode.CONTENT_TOO_LARGE,
        ReplyCode.UNROUTABLE,
        ReplyCode.NO_CONSUMERS,
        ReplyCode.ACCESS_REFUSED,
        ReplyCode.NOT_FOUND,
        ReplyCode.RESOURCE_LOCKED,
        ReplyCode.PRECONDITION_FAILED,
    }
)

HARD_ERRORS = frozenset(
    {
        ReplyCode.CONNECTION_FORCED,
        ReplyCode.INVALID_PATH,
        ReplyCode.FRAME_ERROR,
        ReplyCode.SYNTAX_ERROR,
        ReplyCode.COMMAND_INVALID,
        ReplyCode.CHANNEL_ERROR,
        ReplyCode.UNEXPECTED_FRAME,
        ReplyCode.RESOURCE_ERROR,
        ReplyCode.NOT_ALLOWED,
        ReplyCode.NOT_IMPLEMENTED,
        ReplyCode.INTERNAL_ERROR,
    }
)

FORCED_REPLY_TEXT = "FORCED"
NOT_FORCED_REPLY_TEXT = "NOT_FORCED"

UNPROMPTED_ERRORS = frozenset(
    {
        ReplyCode.CONNECTION_FORCED,
        ReplyCode.INTERNAL_ERROR,
    }
)
UNPROMPTED_CLASS_ID = 0
UNPROMPTED_METHOD_ID = 0


@dataclass(frozen=True, slots=True)
class MessageProperties:
    content_type: str | None = None
    content_encoding: str | None = None
    headers: Table | None = None
    delivery_mode: int | None = None
    priority: int | None = None
    correlation_id: str | None = None
    reply_to: str | None = None
    expiration: str | None = None
    message_id: str | None = None
    timestamp: int | None = None
    type: str | None = None
    user_id: str | None = None
    app_id: str | None = None
    cluster_id: Void = Void()


@dataclass(frozen=True, slots=True)
class RandomTable(Gen[Table]):
    eligible_key_gens: tuple[tuple[str, Gen[TableValue]], ...]

    def from_randomness(self, randomness: Random) -> Table:
        key_gens = randomness.sample(
            self.eligible_key_gens,
            randomness.randint(0, len(self.eligible_key_gens)),
        )
        return Table((key, gen.from_randomness(randomness)) for key, gen in key_gens)


GEN_BIT = NonzeroChance(0.5)
GEN_OCTET = RangedInt(0, OCTET_MAX)
GEN_SHORT = RangedInt(0, SHORT_MAX)
GEN_LONG = RangedInt(0, LONG_MAX)
GEN_ZERO = Constant(0)
GEN_VHOST = Constant(DEFAULT_VHOST)
GEN_FALSE = Constant(False)
GEN_TRUE = Constant(True)
GEN_NONE = Constant(None)
GEN_EMPTY_STR = Constant("")
GEN_EMPTY_BYTES = Constant(b"")
GEN_SHORTSTR = RandomText(0, SHORTSTR_MAX)
GEN_NONEMPTY_SHORTSTR = RandomText(1, SHORTSTR_MAX)
GEN_NAME = RandomText(0, NAME_MAX)
GEN_NONEMPTY_NAME = RandomText(1, NAME_MAX)
GEN_MESSAGE_BODY = RandomBytes(0, MESSAGE_BODY_MAX)
GEN_PRIORITY = RangedInt(PRIORITY_MIN, PRIORITY_MAX)
GEN_DELIVERY_MODE = Choice(DELIVERY_MODES)
GEN_OVERFLOW = Choice(DEFAULT_OVERFLOWS)
GEN_QUEUE_TYPE = Choice(DEFAULT_QUEUE_TYPES)
GEN_EXCHANGE_TYPE = Choice(DEFAULT_EXCHANGE_TYPES)
GEN_CONTENT_TYPE = Choice(DEFAULT_CONTENT_TYPES)
GEN_CONTENT_ENCODING = Choice(DEFAULT_CONTENT_ENCODINGS)
GEN_QUEUE_ARGUMENTS = RandomTable(
    (
        ("x-message-ttl", GEN_LONG),
        ("x-expires", GEN_LONG),
        ("x-max-length", GEN_LONG),
        ("x-max-length-bytes", GEN_LONG),
        ("x-overflow", GEN_OVERFLOW),
        ("x-dead-letter-exchange", GEN_NAME),
        ("x-dead-letter-routing-key", GEN_SHORTSTR),
        ("x-queue-type", GEN_QUEUE_TYPE),
        ("x-single-active-consumer", GEN_BIT),
        ("x-delivery-limit", GEN_LONG),
    )
)
GEN_MESSAGE_HEADERS = RandomTable(
    (
        ("x-machina", GEN_LONG),
        ("x-first-death-queue", GEN_NAME),
        ("x-first-death-exchange", GEN_NAME),
    )
)


@dataclass(frozen=True, slots=True)
class RandomMessageProperties(Gen[MessageProperties | None]):
    ELIGIBLE_KEY_GENS: ClassVar[tuple[tuple[str, Gen[TableValue]], ...]] = (
        ("content_type", GEN_CONTENT_TYPE),
        ("content_encoding", GEN_CONTENT_ENCODING),
        ("headers", GEN_MESSAGE_HEADERS),
        ("delivery_mode", GEN_DELIVERY_MODE),
        ("priority", GEN_PRIORITY),
        ("correlation_id", GEN_NONEMPTY_SHORTSTR),
        ("reply_to", GEN_NONEMPTY_NAME),
        ("expiration", GEN_NONEMPTY_SHORTSTR),
        ("message_id", GEN_NONEMPTY_SHORTSTR),
        ("timestamp", GEN_LONG),
        ("type", GEN_NONEMPTY_SHORTSTR),
        ("user_id", GEN_NONEMPTY_NAME),
        ("app_id", GEN_NONEMPTY_NAME),
    )

    def from_randomness(self, randomness: Random) -> MessageProperties | None:
        key_gens = randomness.sample(
            self.ELIGIBLE_KEY_GENS,
            randomness.randint(0, len(self.ELIGIBLE_KEY_GENS)),
        )
        values: dict[str, Any] = {
            key: gen.from_randomness(randomness) for key, gen in key_gens
        }
        return MessageProperties(**values)


GEN_MESSAGE_PROPERTIES = RandomMessageProperties()
