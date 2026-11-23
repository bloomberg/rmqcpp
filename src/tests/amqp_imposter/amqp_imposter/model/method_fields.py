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
from enum import ReprEnum
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
    MESSAGE_BODY_MAX,
    NAME_MAX,
    PRIORITY_MAX,
    PRIORITY_MIN,
)

from .domains import (
    DecimalValue,
    Long,
    NonEmptyShortStr,
    Octet,
    Short,
    ShortStr,
    SignedLongLong,
    Timestamp,
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
    | SignedLongLong
    | str
    | bytes
    | DecimalValue
    | Timestamp
    | Void
    | Table
    | tuple[TableValue, ...]
)
Table = frozendict[ShortStr, TableValue]


class ClassId(Short, ReprEnum):
    CONNECTION = 10
    CHANNEL = 20
    EXCHANGE = 40
    QUEUE = 50
    BASIC = 60
    CONFIRM = 85


class ReplyCode(Short, ReprEnum):
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

FORCED_REPLY_TEXT = ShortStr("FORCED")
NOT_FORCED_REPLY_TEXT = ShortStr("NOT_FORCED")

UNPROMPTED_ERRORS = frozenset(
    {
        ReplyCode.CONNECTION_FORCED,
        ReplyCode.INTERNAL_ERROR,
    }
)
UNPROMPTED_CLASS_ID = Short(0)
UNPROMPTED_METHOD_ID = Short(0)


@dataclass(frozen=True, slots=True)
class MessageProperties:
    content_type: ShortStr | None = None
    content_encoding: ShortStr | None = None
    headers: Table | None = None
    delivery_mode: Octet | None = None
    priority: Octet | None = None
    correlation_id: ShortStr | None = None
    reply_to: ShortStr | None = None
    expiration: ShortStr | None = None
    message_id: ShortStr | None = None
    timestamp: Timestamp | None = None
    type: ShortStr | None = None
    user_id: ShortStr | None = None
    app_id: ShortStr | None = None
    cluster_id: Void = Void()


@dataclass(frozen=True, slots=True)
class RandomTable(Gen[Table]):
    eligible_key_gens: tuple[tuple[ShortStr, Gen[TableValue]], ...]

    def from_randomness(self, randomness: Random) -> Table:
        key_gens = randomness.sample(
            self.eligible_key_gens,
            randomness.randint(0, len(self.eligible_key_gens)),
        )
        return Table((key, gen.from_randomness(randomness)) for key, gen in key_gens)


GEN_BIT = NonzeroChance(0.5)
GEN_OCTET = RangedInt(Octet, 0, Octet.MAX)
GEN_SHORT = RangedInt(Short, 0, Short.MAX)
GEN_LONG = RangedInt(Long, 0, Long.MAX)
GEN_TIMESTAMP = RangedInt(Timestamp, 0, Long.MAX)
GEN_TABLE_LONG = RangedInt(SignedLongLong, 0, Long.MAX)
GEN_ZERO_SHORT = Constant(Short(0))
GEN_ZERO_LONG = Constant(Long(0))
GEN_VHOST = Constant(DEFAULT_VHOST)
GEN_FALSE = Constant(False)
GEN_TRUE = Constant(True)
GEN_EMPTY_TABLE = Constant(Table())
GEN_EMPTY_PROPERTIES = Constant(MessageProperties())
GEN_EMPTY_STR = Constant("")
GEN_EMPTY_SHORTSTR = Constant(ShortStr(""))
GEN_EMPTY_BYTES = Constant(b"")
GEN_SHORTSTR = RandomText(ShortStr, 0, ShortStr.MAX_OCTETS)
GEN_NONEMPTY_SHORTSTR = RandomText(NonEmptyShortStr, 1, ShortStr.MAX_OCTETS)
GEN_NAME = RandomText(ShortStr, 0, NAME_MAX)
GEN_NONEMPTY_NAME = RandomText(NonEmptyShortStr, 1, NAME_MAX)
GEN_MESSAGE_BODY = RandomBytes(0, MESSAGE_BODY_MAX)
GEN_PRIORITY = RangedInt(Octet, PRIORITY_MIN, PRIORITY_MAX)
GEN_DELIVERY_MODE = Choice(DELIVERY_MODES)
GEN_OVERFLOW = Choice(DEFAULT_OVERFLOWS)
GEN_QUEUE_TYPE = Choice(DEFAULT_QUEUE_TYPES)
GEN_EXCHANGE_TYPE = Choice(DEFAULT_EXCHANGE_TYPES)
GEN_CONTENT_TYPE = Choice(DEFAULT_CONTENT_TYPES)
GEN_CONTENT_ENCODING = Choice(DEFAULT_CONTENT_ENCODINGS)
GEN_QUEUE_ARGUMENTS = RandomTable(
    (
        (ShortStr("x-message-ttl"), GEN_TABLE_LONG),
        (ShortStr("x-expires"), GEN_TABLE_LONG),
        (ShortStr("x-max-length"), GEN_TABLE_LONG),
        (ShortStr("x-max-length-bytes"), GEN_TABLE_LONG),
        (ShortStr("x-overflow"), GEN_OVERFLOW),
        (ShortStr("x-dead-letter-exchange"), GEN_NAME),
        (ShortStr("x-dead-letter-routing-key"), GEN_SHORTSTR),
        (ShortStr("x-queue-type"), GEN_QUEUE_TYPE),
        (ShortStr("x-single-active-consumer"), GEN_BIT),
        (ShortStr("x-delivery-limit"), GEN_TABLE_LONG),
    )
)
GEN_MESSAGE_HEADERS = RandomTable(
    (
        (ShortStr("x-machina"), GEN_TABLE_LONG),
        (ShortStr("x-first-death-queue"), GEN_NAME),
        (ShortStr("x-first-death-exchange"), GEN_NAME),
    )
)


@dataclass(frozen=True, slots=True)
class RandomMessageProperties(Gen[MessageProperties]):
    ELIGIBLE_KEY_GENS: ClassVar[tuple[tuple[str, Gen[object]], ...]] = (
        ("content_type", GEN_CONTENT_TYPE),
        ("content_encoding", GEN_CONTENT_ENCODING),
        ("headers", GEN_MESSAGE_HEADERS),
        ("delivery_mode", GEN_DELIVERY_MODE),
        ("priority", GEN_PRIORITY),
        ("correlation_id", GEN_NONEMPTY_SHORTSTR),
        ("reply_to", GEN_NONEMPTY_NAME),
        ("expiration", GEN_NONEMPTY_SHORTSTR),
        ("message_id", GEN_NONEMPTY_SHORTSTR),
        ("timestamp", GEN_TIMESTAMP),
        ("type", GEN_NONEMPTY_SHORTSTR),
        ("user_id", GEN_NONEMPTY_NAME),
        ("app_id", GEN_NONEMPTY_NAME),
    )

    def from_randomness(self, randomness: Random) -> MessageProperties:
        key_gens = randomness.sample(
            self.ELIGIBLE_KEY_GENS,
            randomness.randint(0, len(self.ELIGIBLE_KEY_GENS)),
        )
        values: dict[str, Any] = {
            key: gen.from_randomness(randomness) for key, gen in key_gens
        }
        return MessageProperties(**values)


GEN_MESSAGE_PROPERTIES = RandomMessageProperties()
