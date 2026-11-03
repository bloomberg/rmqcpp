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

from dataclasses import dataclass, fields
from typing import Any, ClassVar, TypeIs, get_args

from .method_fields import (
    GEN_BIT,
    GEN_EMPTY_BYTES,
    GEN_EMPTY_STR,
    GEN_EXCHANGE_TYPE,
    GEN_FALSE,
    GEN_LONG,
    GEN_MESSAGE_BODY,
    GEN_MESSAGE_PROPERTIES,
    GEN_NAME,
    GEN_NONE,
    GEN_NONEMPTY_NAME,
    GEN_NONEMPTY_SHORTSTR,
    GEN_SHORTSTR,
    GEN_VHOST,
    GEN_ZERO,
    HOLE,
    Arbitrary,
    ClassId,
    Exact,
    MessageProperties,
    Table,
    is_concrete,
)


@dataclass(frozen=True, slots=True)
class ConnectionStart:
    CLASS_ID: ClassVar[int] = ClassId.CONNECTION
    METHOD_ID: ClassVar[int] = 10

    version_major: Exact[int] = HOLE
    version_minor: Exact[int] = HOLE
    mechanisms: Exact[tuple[str, ...]] = HOLE
    locales: Exact[tuple[str, ...]] = HOLE
    publisher_confirms: Exact[bool] = HOLE
    basic_nack: Exact[bool] = HOLE
    consumer_cancel_notify: Exact[bool] = HOLE


@dataclass(frozen=True, slots=True)
class ConnectionStartOk:
    CLASS_ID: ClassVar[int] = ClassId.CONNECTION
    METHOD_ID: ClassVar[int] = 11

    mechanism: Exact[str] = HOLE
    locale: Exact[str] = HOLE
    response: Arbitrary[str] = GEN_EMPTY_STR


@dataclass(frozen=True, slots=True)
class ConnectionTune:
    CLASS_ID: ClassVar[int] = ClassId.CONNECTION
    METHOD_ID: ClassVar[int] = 30

    channel_max: Exact[int] = HOLE
    frame_max: Exact[int] = HOLE
    heartbeat: Exact[int] = HOLE


@dataclass(frozen=True, slots=True)
class ConnectionTuneOk:
    CLASS_ID: ClassVar[int] = ClassId.CONNECTION
    METHOD_ID: ClassVar[int] = 31

    channel_max: Exact[int] = HOLE
    frame_max: Exact[int] = HOLE
    heartbeat: Exact[int] = HOLE


@dataclass(frozen=True, slots=True)
class ConnectionOpen:
    CLASS_ID: ClassVar[int] = ClassId.CONNECTION
    METHOD_ID: ClassVar[int] = 40

    vhost: Arbitrary[str] = GEN_VHOST


@dataclass(frozen=True, slots=True)
class ConnectionOpenOk:
    CLASS_ID: ClassVar[int] = ClassId.CONNECTION
    METHOD_ID: ClassVar[int] = 41


@dataclass(frozen=True, slots=True)
class ConnectionClose:
    CLASS_ID: ClassVar[int] = ClassId.CONNECTION
    METHOD_ID: ClassVar[int] = 50

    reply_code: Exact[int] = HOLE
    reply_text: Exact[str] = HOLE
    class_id: Exact[int] = HOLE
    method_id: Exact[int] = HOLE


@dataclass(frozen=True, slots=True)
class ConnectionCloseOk:
    CLASS_ID: ClassVar[int] = ClassId.CONNECTION
    METHOD_ID: ClassVar[int] = 51


@dataclass(frozen=True, slots=True)
class ChannelOpen:
    CLASS_ID: ClassVar[int] = ClassId.CHANNEL
    METHOD_ID: ClassVar[int] = 10


@dataclass(frozen=True, slots=True)
class ChannelOpenOk:
    CLASS_ID: ClassVar[int] = ClassId.CHANNEL
    METHOD_ID: ClassVar[int] = 11


@dataclass(frozen=True, slots=True)
class ChannelClose:
    CLASS_ID: ClassVar[int] = ClassId.CHANNEL
    METHOD_ID: ClassVar[int] = 40

    reply_code: Exact[int] = HOLE
    reply_text: Exact[str] = HOLE
    class_id: Exact[int] = HOLE
    method_id: Exact[int] = HOLE


@dataclass(frozen=True, slots=True)
class ChannelCloseOk:
    CLASS_ID: ClassVar[int] = ClassId.CHANNEL
    METHOD_ID: ClassVar[int] = 41


@dataclass(frozen=True, slots=True)
class ConfirmSelect:
    CLASS_ID: ClassVar[int] = ClassId.CONFIRM
    METHOD_ID: ClassVar[int] = 10

    no_wait: Arbitrary[bool] = (
        GEN_FALSE  # instantiations with this True need to be hand-crafted
    )


@dataclass(frozen=True, slots=True)
class ConfirmSelectOk:
    CLASS_ID: ClassVar[int] = ClassId.CONFIRM
    METHOD_ID: ClassVar[int] = 11


@dataclass(frozen=True, slots=True)
class ExchangeDeclare:
    CLASS_ID: ClassVar[int] = ClassId.EXCHANGE
    METHOD_ID: ClassVar[int] = 10

    exchange: Exact[str] = HOLE
    exchange_type: Arbitrary[str] = GEN_EXCHANGE_TYPE
    passive: Arbitrary[bool] = GEN_FALSE
    durable: Arbitrary[bool] = GEN_FALSE
    auto_delete: Arbitrary[bool] = GEN_FALSE
    internal: Arbitrary[bool] = GEN_FALSE
    no_wait: Arbitrary[bool] = GEN_FALSE
    arguments: Arbitrary[Table | None] = GEN_NONE


@dataclass(frozen=True, slots=True)
class ExchangeDeclareOk:
    CLASS_ID: ClassVar[int] = ClassId.EXCHANGE
    METHOD_ID: ClassVar[int] = 11


@dataclass(frozen=True, slots=True)
class ExchangeBind:
    CLASS_ID: ClassVar[int] = ClassId.EXCHANGE
    METHOD_ID: ClassVar[int] = 30

    destination: Arbitrary[str] = GEN_NONEMPTY_NAME
    source: Arbitrary[str] = GEN_NONEMPTY_NAME
    routing_key: Arbitrary[str] = GEN_SHORTSTR
    no_wait: Arbitrary[bool] = GEN_FALSE
    arguments: Arbitrary[Table | None] = GEN_NONE


@dataclass(frozen=True, slots=True)
class ExchangeBindOk:
    CLASS_ID: ClassVar[int] = ClassId.EXCHANGE
    METHOD_ID: ClassVar[int] = 31


@dataclass(frozen=True, slots=True)
class QueueDeclare:
    CLASS_ID: ClassVar[int] = ClassId.QUEUE
    METHOD_ID: ClassVar[int] = 10

    queue: Arbitrary[str] = GEN_NONEMPTY_NAME
    passive: Arbitrary[bool] = GEN_FALSE
    durable: Arbitrary[bool] = GEN_FALSE
    exclusive: Arbitrary[bool] = GEN_FALSE
    auto_delete: Arbitrary[bool] = GEN_FALSE
    no_wait: Arbitrary[bool] = GEN_FALSE
    arguments: Arbitrary[Table | None] = GEN_NONE


@dataclass(frozen=True, slots=True)
class QueueDeclareOk:
    CLASS_ID: ClassVar[int] = ClassId.QUEUE
    METHOD_ID: ClassVar[int] = 11

    queue: Arbitrary[str] = GEN_NONEMPTY_NAME  # can be anonymous
    message_count: Arbitrary[int] = GEN_LONG
    consumer_count: Arbitrary[int] = GEN_LONG


@dataclass(frozen=True, slots=True)
class QueueBind:
    CLASS_ID: ClassVar[int] = ClassId.QUEUE
    METHOD_ID: ClassVar[int] = 20

    queue: Arbitrary[str] = GEN_NONEMPTY_NAME
    exchange: Arbitrary[str] = GEN_NONEMPTY_NAME
    routing_key: Arbitrary[str] = GEN_SHORTSTR
    no_wait: Arbitrary[bool] = GEN_FALSE
    arguments: Arbitrary[Table | None] = GEN_NONE


@dataclass(frozen=True, slots=True)
class QueueBindOk:
    CLASS_ID: ClassVar[int] = ClassId.QUEUE
    METHOD_ID: ClassVar[int] = 21


@dataclass(frozen=True, slots=True)
class BasicQos:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 10

    prefetch_count: Arbitrary[int] = GEN_ZERO
    prefetch_size: Arbitrary[int] = GEN_ZERO  # nonzero produces "not implemented"
    global_qos: Arbitrary[bool] = GEN_FALSE


@dataclass(frozen=True, slots=True)
class BasicQosOk:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 11


@dataclass(frozen=True, slots=True)
class BasicConsume:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 20

    queue: Arbitrary[str] = GEN_NONEMPTY_NAME
    consumer_tag: Arbitrary[str] = GEN_EMPTY_STR
    no_local: Arbitrary[bool] = GEN_FALSE
    no_ack: Arbitrary[bool] = GEN_FALSE
    no_wait: Arbitrary[bool] = GEN_FALSE
    exclusive: Arbitrary[bool] = GEN_FALSE
    arguments: Arbitrary[Table | None] = GEN_NONE


@dataclass(frozen=True, slots=True)
class BasicConsumeOk:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 21

    consumer_tag: Arbitrary[str] = GEN_NONEMPTY_SHORTSTR


@dataclass(frozen=True, slots=True)
class BasicCancel:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 30

    consumer_tag: Exact[str] = HOLE
    no_wait: Arbitrary[bool] = GEN_FALSE


@dataclass(frozen=True, slots=True)
class BasicCancelOk:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 31

    consumer_tag: Exact[str] = HOLE


@dataclass(frozen=True, slots=True)
class BasicPublish:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 40

    exchange: Arbitrary[str] = GEN_NAME
    routing_key: Arbitrary[str] = GEN_SHORTSTR
    mandatory: Arbitrary[bool] = GEN_FALSE
    immediate: Arbitrary[bool] = GEN_FALSE  # true produces "not implemented"
    properties: Arbitrary[MessageProperties | None] = GEN_NONE
    body: Arbitrary[bytes] = GEN_MESSAGE_BODY


@dataclass(frozen=True, slots=True)
class BasicReturn:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 50

    reply_code: Exact[int] = HOLE
    reply_text: Arbitrary[str] = GEN_NONEMPTY_SHORTSTR
    exchange: Exact[str] = HOLE
    routing_key: Exact[str] = HOLE
    properties: Arbitrary[MessageProperties | None] = GEN_NONE
    body: Arbitrary[bytes] = GEN_EMPTY_BYTES


@dataclass(frozen=True, slots=True)
class BasicAck:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 80

    delivery_tag: Exact[int] = HOLE
    multiple: Arbitrary[bool] = GEN_FALSE


@dataclass(frozen=True, slots=True)
class BasicNack:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 120

    delivery_tag: Exact[int] = HOLE
    multiple: Arbitrary[bool] = GEN_FALSE
    requeue: Arbitrary[bool] = GEN_FALSE


@dataclass(frozen=True, slots=True)
class BasicReject:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 90

    delivery_tag: Exact[int] = HOLE
    requeue: Arbitrary[bool] = GEN_FALSE


@dataclass(frozen=True, slots=True)
class BasicDeliver:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 60

    consumer_tag: Exact[str] = HOLE
    delivery_tag: Exact[int] = HOLE
    redelivered: Arbitrary[bool] = GEN_BIT
    exchange: Arbitrary[str] = GEN_NONEMPTY_NAME
    routing_key: Arbitrary[str] = GEN_SHORTSTR
    properties: Arbitrary[MessageProperties | None] = GEN_MESSAGE_PROPERTIES
    body: Arbitrary[bytes] = GEN_MESSAGE_BODY


@dataclass(frozen=True, slots=True)
class BasicGet:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 70

    queue: Arbitrary[str] = GEN_NONEMPTY_NAME
    no_ack: Arbitrary[bool] = GEN_FALSE


@dataclass(frozen=True, slots=True)
class BasicGetOk:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 71

    delivery_tag: Exact[int] = HOLE
    redelivered: Arbitrary[bool] = GEN_BIT
    exchange: Arbitrary[str] = GEN_NONEMPTY_NAME
    routing_key: Arbitrary[str] = GEN_SHORTSTR
    message_count: Arbitrary[int] = GEN_LONG
    properties: Arbitrary[MessageProperties | None] = GEN_MESSAGE_PROPERTIES
    body: Arbitrary[bytes] = GEN_MESSAGE_BODY


@dataclass(frozen=True, slots=True)
class BasicGetEmpty:
    CLASS_ID: ClassVar[int] = ClassId.BASIC
    METHOD_ID: ClassVar[int] = 72


type Method = (
    ConnectionStart
    | ConnectionStartOk
    | ConnectionTune
    | ConnectionTuneOk
    | ConnectionOpen
    | ConnectionOpenOk
    | ConnectionClose
    | ConnectionCloseOk
    | ChannelOpen
    | ChannelOpenOk
    | ChannelClose
    | ChannelCloseOk
    | ConfirmSelect
    | ConfirmSelectOk
    | ExchangeDeclare
    | ExchangeDeclareOk
    | ExchangeBind
    | ExchangeBindOk
    | QueueDeclare
    | QueueDeclareOk
    | QueueBind
    | QueueBindOk
    | BasicQos
    | BasicQosOk
    | BasicConsume
    | BasicConsumeOk
    | BasicCancel
    | BasicCancelOk
    | BasicPublish
    | BasicReturn
    | BasicAck
    | BasicNack
    | BasicReject
    | BasicDeliver
    | BasicGet
    | BasicGetOk
    | BasicGetEmpty
)


METHOD_KINDS: tuple[type[Method], ...] = get_args(Method.__value__)


def is_same_kind[M: Method](other: Method, method: M) -> TypeIs[M]:
    if (other.CLASS_ID, other.METHOD_ID) == (method.CLASS_ID, method.METHOD_ID):
        assert type(other) is type(method)
        return True
    return False


def distinct_kinds(methods: tuple[Method, ...]) -> tuple[type[Method], ...]:
    return tuple(dict.fromkeys(type(method) for method in methods))


def exact_fields(kind: type[Method]) -> tuple[str, ...]:
    return tuple(field.name for field in fields(kind) if field.default is HOLE)


def arbitrary_fields(method: Method) -> tuple[str, ...]:
    return tuple(
        field.name
        for field in fields(method)
        if field.default is not HOLE and is_concrete(getattr(method, field.name))
    )


class BadPattern(ValueError):
    pass


def validate_pattern(pattern: Method) -> None:
    if names := arbitrary_fields(pattern):
        raise BadPattern(f"{type(pattern).__name__} cannot be matched on {names}")


def set_fields[M: Method](method: M, fields: dict[str, Any]) -> M:
    return type(method)(**fields)


def get_fields(method: Method) -> dict[str, Any]:
    return {field.name: getattr(method, field.name) for field in fields(method)}


def is_method_pattern(method: Method) -> bool:
    return any(getattr(method, field.name) is HOLE for field in fields(method))
