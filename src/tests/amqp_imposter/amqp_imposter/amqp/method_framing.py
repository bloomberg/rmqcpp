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

import struct
from collections.abc import Mapping
from dataclasses import dataclass
from typing import Any, TypeIs, assert_never, cast

import pika.frame as pf
import pika.spec as ps
from pika.exceptions import InvalidFrameError

from ..model import method_fields as mf
from ..model import method_unification as mu
from ..model import methods as m
from ..model.generators import DEFAULT_GEN_STATE

PROTOCOL_HEADER_091 = b"AMQP\x00\x00\x09\x01"
AMQP_TUPLE_DELIMITER = " "

CAPABILITIES = "capabilities"
PUBLISHER_CONFIRMS = "publisher_confirms"
BASIC_NACK = "basic.nack"
CONSUMER_CANCEL_NOTIFY = "consumer_cancel_notify"


@dataclass(frozen=True, slots=True)
class ProtocolHeader:
    wire_bytes: bytes


@dataclass(frozen=True, slots=True)
class MethodOnChannel:
    channel: int
    method: m.Method


@dataclass(frozen=True, slots=True)
class Heartbeat:
    pass


@dataclass(frozen=True, slots=True)
class Undecodable:
    reason: str


type Ingress = ProtocolHeader | MethodOnChannel | Heartbeat | Undecodable
type Egress = Heartbeat | MethodOnChannel


def is_content_method(
    method: m.Method,
) -> TypeIs[m.BasicPublish | m.BasicReturn | m.BasicDeliver | m.BasicGetOk]:
    match method:
        case m.BasicPublish() | m.BasicReturn() | m.BasicDeliver() | m.BasicGetOk():
            return True
        case _:
            return False


def is_pika_content_method(
    spec: Any,
) -> TypeIs[ps.Basic.Publish | ps.Basic.Return | ps.Basic.Deliver | ps.Basic.GetOk]:
    match spec:
        case (
            ps.Basic.Publish()
            | ps.Basic.Return()
            | ps.Basic.Deliver()
            | ps.Basic.GetOk()
        ):
            return True
        case _:
            return False


def to_pika_value(value: Any) -> Any:
    match value:
        case mf.Void():
            return None
        case frozendict():
            return {key: to_pika_value(one) for key, one in value.items()}
        case tuple():
            return [to_pika_value(one) for one in value]
        case _:
            return value


def from_pika_value(value: Any) -> Any:
    match value:
        case None:
            return mf.Void()
        case dict():
            return frozendict((key, from_pika_value(one)) for key, one in value.items())
        case list():
            return tuple(from_pika_value(one) for one in value)
        case _:
            return value


def to_text(value: str | bytes | None) -> str:
    match value:
        case None:
            return ""
        case bytes():
            return value.decode()
        case _:
            return value


def from_amqp_tuple(value: str | bytes | None) -> tuple[str, ...]:
    text = to_text(value)

    return tuple(text.split(AMQP_TUPLE_DELIMITER)) if text else ()


def capabilities_of(spec: Any) -> Mapping[str, Any]:
    match (spec.server_properties or {}).get(CAPABILITIES):
        case Mapping() as capabilities:
            return capabilities
        case _:
            return {}


def to_pika_properties(properties: mf.MessageProperties | None) -> ps.BasicProperties:
    if properties is None:
        return ps.BasicProperties()

    return ps.BasicProperties(
        content_type=properties.content_type,
        content_encoding=properties.content_encoding,
        delivery_mode=cast(Any, properties.delivery_mode),
        priority=properties.priority,
        correlation_id=properties.correlation_id,
        reply_to=properties.reply_to,
        expiration=properties.expiration,
        message_id=properties.message_id,
        timestamp=properties.timestamp,
        type=properties.type,
        user_id=properties.user_id,
        app_id=properties.app_id,
        headers=to_pika_value(properties.headers)
        if properties.headers is not None
        else None,
    )


def from_properties(properties: ps.BasicProperties | None) -> mf.MessageProperties:
    if properties is None:
        return mf.MessageProperties()

    return mf.MessageProperties(
        content_type=properties.content_type,
        content_encoding=properties.content_encoding,
        delivery_mode=cast(Any, properties.delivery_mode),
        priority=properties.priority,
        correlation_id=properties.correlation_id,
        reply_to=properties.reply_to,
        expiration=properties.expiration,
        message_id=properties.message_id,
        timestamp=properties.timestamp,
        type=properties.type,
        user_id=properties.user_id,
        app_id=properties.app_id,
        headers=from_pika_value(properties.headers)
        if properties.headers is not None
        else None,
    )


def to_pika(method: m.Method) -> Any:
    match method:
        case m.ConnectionStart():
            return ps.Connection.Start(
                version_major=mf.assert_concrete(method.version_major),
                version_minor=mf.assert_concrete(method.version_minor),
                server_properties={
                    CAPABILITIES: {
                        PUBLISHER_CONFIRMS: mf.assert_concrete(
                            method.publisher_confirms
                        ),
                        BASIC_NACK: mf.assert_concrete(method.basic_nack),
                        CONSUMER_CANCEL_NOTIFY: mf.assert_concrete(
                            method.consumer_cancel_notify
                        ),
                    }
                },
                mechanisms=AMQP_TUPLE_DELIMITER.join(
                    mf.assert_concrete(method.mechanisms)
                ),
                locales=AMQP_TUPLE_DELIMITER.join(mf.assert_concrete(method.locales)),
            )
        case m.ConnectionStartOk():
            return ps.Connection.StartOk(
                mechanism=mf.assert_concrete(method.mechanism),
                locale=mf.assert_concrete(method.locale),
                response=mf.assert_concrete(method.response),
            )
        case m.ConnectionTune():
            return ps.Connection.Tune(
                channel_max=mf.assert_concrete(method.channel_max),
                frame_max=mf.assert_concrete(method.frame_max),
                heartbeat=mf.assert_concrete(method.heartbeat),
            )
        case m.ConnectionTuneOk():
            return ps.Connection.TuneOk(
                channel_max=mf.assert_concrete(method.channel_max),
                frame_max=mf.assert_concrete(method.frame_max),
                heartbeat=mf.assert_concrete(method.heartbeat),
            )
        case m.ConnectionOpen():
            return ps.Connection.Open(
                virtual_host=mf.assert_concrete(method.vhost),
            )
        case m.ConnectionOpenOk():
            return ps.Connection.OpenOk()
        case m.ConnectionClose():
            return ps.Connection.Close(
                reply_code=mf.assert_concrete(method.reply_code),
                reply_text=mf.assert_concrete(method.reply_text),
                class_id=mf.assert_concrete(method.class_id),
                method_id=mf.assert_concrete(method.method_id),
            )
        case m.ConnectionCloseOk():
            return ps.Connection.CloseOk()
        case m.ChannelOpen():
            return ps.Channel.Open()
        case m.ChannelOpenOk():
            return ps.Channel.OpenOk()
        case m.ChannelClose():
            return ps.Channel.Close(
                reply_code=mf.assert_concrete(method.reply_code),
                reply_text=mf.assert_concrete(method.reply_text),
                class_id=mf.assert_concrete(method.class_id),
                method_id=mf.assert_concrete(method.method_id),
            )
        case m.ChannelCloseOk():
            return ps.Channel.CloseOk()
        case m.ConfirmSelect():
            return ps.Confirm.Select(
                nowait=mf.assert_concrete(method.no_wait),
            )
        case m.ConfirmSelectOk():
            return ps.Confirm.SelectOk()
        case m.ExchangeDeclare():
            return ps.Exchange.Declare(
                exchange=mf.assert_concrete(method.exchange),
                type=mf.assert_concrete(method.exchange_type),
                passive=mf.assert_concrete(method.passive),
                durable=mf.assert_concrete(method.durable),
                auto_delete=mf.assert_concrete(method.auto_delete),
                internal=mf.assert_concrete(method.internal),
                nowait=mf.assert_concrete(method.no_wait),
                arguments=to_pika_value(mf.assert_concrete(method.arguments)),
            )
        case m.ExchangeDeclareOk():
            return ps.Exchange.DeclareOk()
        case m.ExchangeBind():
            return ps.Exchange.Bind(
                destination=mf.assert_concrete(method.destination),
                source=mf.assert_concrete(method.source),
                routing_key=mf.assert_concrete(method.routing_key),
                nowait=mf.assert_concrete(method.no_wait),
                arguments=to_pika_value(mf.assert_concrete(method.arguments)),
            )
        case m.ExchangeBindOk():
            return ps.Exchange.BindOk()
        case m.QueueDeclare():
            return ps.Queue.Declare(
                queue=mf.assert_concrete(method.queue),
                passive=mf.assert_concrete(method.passive),
                durable=mf.assert_concrete(method.durable),
                exclusive=mf.assert_concrete(method.exclusive),
                auto_delete=mf.assert_concrete(method.auto_delete),
                nowait=mf.assert_concrete(method.no_wait),
                arguments=to_pika_value(mf.assert_concrete(method.arguments)),
            )
        case m.QueueDeclareOk():
            return ps.Queue.DeclareOk(
                queue=mf.assert_concrete(method.queue),
                message_count=mf.assert_concrete(method.message_count),
                consumer_count=mf.assert_concrete(method.consumer_count),
            )
        case m.QueueBind():
            return ps.Queue.Bind(
                queue=mf.assert_concrete(method.queue),
                exchange=mf.assert_concrete(method.exchange),
                routing_key=mf.assert_concrete(method.routing_key),
                nowait=mf.assert_concrete(method.no_wait),
                arguments=to_pika_value(mf.assert_concrete(method.arguments)),
            )
        case m.QueueBindOk():
            return ps.Queue.BindOk()
        case m.BasicQos():
            return ps.Basic.Qos(
                prefetch_count=mf.assert_concrete(method.prefetch_count),
                prefetch_size=mf.assert_concrete(method.prefetch_size),
                global_qos=mf.assert_concrete(method.global_qos),
            )
        case m.BasicQosOk():
            return ps.Basic.QosOk()
        case m.BasicConsume():
            return ps.Basic.Consume(
                queue=mf.assert_concrete(method.queue),
                consumer_tag=mf.assert_concrete(method.consumer_tag),
                no_local=mf.assert_concrete(method.no_local),
                no_ack=mf.assert_concrete(method.no_ack),
                nowait=mf.assert_concrete(method.no_wait),
                exclusive=mf.assert_concrete(method.exclusive),
                arguments=to_pika_value(mf.assert_concrete(method.arguments)),
            )
        case m.BasicConsumeOk():
            return ps.Basic.ConsumeOk(
                consumer_tag=mf.assert_concrete(method.consumer_tag),
            )
        case m.BasicCancel():
            return ps.Basic.Cancel(
                consumer_tag=mf.assert_concrete(method.consumer_tag),
                nowait=mf.assert_concrete(method.no_wait),
            )
        case m.BasicCancelOk():
            return ps.Basic.CancelOk(
                consumer_tag=mf.assert_concrete(method.consumer_tag),
            )
        case m.BasicPublish():
            return ps.Basic.Publish(
                exchange=mf.assert_concrete(method.exchange),
                routing_key=mf.assert_concrete(method.routing_key),
                mandatory=mf.assert_concrete(method.mandatory),
                immediate=mf.assert_concrete(method.immediate),
            )
        case m.BasicReturn():
            return ps.Basic.Return(
                reply_code=mf.assert_concrete(method.reply_code),
                reply_text=mf.assert_concrete(method.reply_text),
                exchange=mf.assert_concrete(method.exchange),
                routing_key=mf.assert_concrete(method.routing_key),
            )
        case m.BasicAck():
            return ps.Basic.Ack(
                delivery_tag=mf.assert_concrete(method.delivery_tag),
                multiple=mf.assert_concrete(method.multiple),
            )
        case m.BasicNack():
            return ps.Basic.Nack(
                delivery_tag=mf.assert_concrete(method.delivery_tag),
                multiple=mf.assert_concrete(method.multiple),
                requeue=mf.assert_concrete(method.requeue),
            )
        case m.BasicReject():
            return ps.Basic.Reject(
                delivery_tag=mf.assert_concrete(method.delivery_tag),
                requeue=mf.assert_concrete(method.requeue),
            )
        case m.BasicDeliver():
            return ps.Basic.Deliver(
                consumer_tag=mf.assert_concrete(method.consumer_tag),
                delivery_tag=mf.assert_concrete(method.delivery_tag),
                redelivered=mf.assert_concrete(method.redelivered),
                exchange=mf.assert_concrete(method.exchange),
                routing_key=mf.assert_concrete(method.routing_key),
            )
        case m.BasicGet():
            return ps.Basic.Get(
                queue=mf.assert_concrete(method.queue),
                no_ack=mf.assert_concrete(method.no_ack),
            )
        case m.BasicGetOk():
            return ps.Basic.GetOk(
                delivery_tag=mf.assert_concrete(method.delivery_tag),
                redelivered=mf.assert_concrete(method.redelivered),
                exchange=mf.assert_concrete(method.exchange),
                routing_key=mf.assert_concrete(method.routing_key),
                message_count=mf.assert_concrete(method.message_count),
            )
        case m.BasicGetEmpty():
            return ps.Basic.GetEmpty()
        case _:
            assert_never(method)


def from_pika(
    spec: Any, properties: ps.BasicProperties | None = None, body: bytes = b""
) -> m.Method | None:
    match spec:
        case ps.Connection.Start():
            capabilities = capabilities_of(spec)

            return m.ConnectionStart(
                version_major=spec.version_major,
                version_minor=spec.version_minor,
                mechanisms=from_amqp_tuple(spec.mechanisms),
                locales=from_amqp_tuple(spec.locales),
                publisher_confirms=bool(capabilities.get(PUBLISHER_CONFIRMS, False)),
                basic_nack=bool(capabilities.get(BASIC_NACK, False)),
                consumer_cancel_notify=bool(
                    capabilities.get(CONSUMER_CANCEL_NOTIFY, False)
                ),
            )
        case ps.Connection.StartOk():
            return m.ConnectionStartOk(
                mechanism=spec.mechanism,
                locale=spec.locale,
                response=to_text(spec.response),
            )
        case ps.Connection.Tune():
            return m.ConnectionTune(
                channel_max=spec.channel_max,
                frame_max=spec.frame_max,
                heartbeat=spec.heartbeat,
            )
        case ps.Connection.TuneOk():
            return m.ConnectionTuneOk(
                channel_max=spec.channel_max,
                frame_max=spec.frame_max,
                heartbeat=spec.heartbeat,
            )
        case ps.Connection.Open():
            return m.ConnectionOpen(
                vhost=spec.virtual_host,
            )
        case ps.Connection.OpenOk():
            return m.ConnectionOpenOk()
        case ps.Connection.Close():
            return m.ConnectionClose(
                reply_code=spec.reply_code,
                reply_text=spec.reply_text,
                class_id=spec.class_id,
                method_id=spec.method_id,
            )
        case ps.Connection.CloseOk():
            return m.ConnectionCloseOk()
        case ps.Channel.Open():
            return m.ChannelOpen()
        case ps.Channel.OpenOk():
            return m.ChannelOpenOk()
        case ps.Channel.Close():
            return m.ChannelClose(
                reply_code=spec.reply_code,
                reply_text=spec.reply_text,
                class_id=spec.class_id,
                method_id=spec.method_id,
            )
        case ps.Channel.CloseOk():
            return m.ChannelCloseOk()
        case ps.Confirm.Select():
            return m.ConfirmSelect(
                no_wait=spec.nowait,
            )
        case ps.Confirm.SelectOk():
            return m.ConfirmSelectOk()
        case ps.Exchange.Declare():
            return m.ExchangeDeclare(
                exchange=spec.exchange,
                exchange_type=spec.type,
                passive=spec.passive,
                durable=spec.durable,
                auto_delete=spec.auto_delete,
                internal=spec.internal,
                no_wait=spec.nowait,
                arguments=from_pika_value(spec.arguments),
            )
        case ps.Exchange.DeclareOk():
            return m.ExchangeDeclareOk()
        case ps.Exchange.Bind():
            return m.ExchangeBind(
                destination=to_text(spec.destination),
                source=to_text(spec.source),
                routing_key=spec.routing_key,
                no_wait=spec.nowait,
                arguments=from_pika_value(spec.arguments),
            )
        case ps.Exchange.BindOk():
            return m.ExchangeBindOk()
        case ps.Queue.Declare():
            return m.QueueDeclare(
                queue=spec.queue,
                passive=spec.passive,
                durable=spec.durable,
                exclusive=spec.exclusive,
                auto_delete=spec.auto_delete,
                no_wait=spec.nowait,
                arguments=from_pika_value(spec.arguments),
            )
        case ps.Queue.DeclareOk():
            return m.QueueDeclareOk(
                queue=spec.queue,
                message_count=spec.message_count,
                consumer_count=spec.consumer_count,
            )
        case ps.Queue.Bind():
            return m.QueueBind(
                queue=spec.queue,
                exchange=spec.exchange,
                routing_key=spec.routing_key,
                no_wait=spec.nowait,
                arguments=from_pika_value(spec.arguments),
            )
        case ps.Queue.BindOk():
            return m.QueueBindOk()
        case ps.Basic.Qos():
            return m.BasicQos(
                prefetch_count=spec.prefetch_count,
                prefetch_size=spec.prefetch_size,
                global_qos=spec.global_qos,
            )
        case ps.Basic.QosOk():
            return m.BasicQosOk()
        case ps.Basic.Consume():
            return m.BasicConsume(
                queue=spec.queue,
                consumer_tag=spec.consumer_tag,
                no_local=spec.no_local,
                no_ack=spec.no_ack,
                no_wait=spec.nowait,
                exclusive=spec.exclusive,
                arguments=from_pika_value(spec.arguments),
            )
        case ps.Basic.ConsumeOk():
            return m.BasicConsumeOk(
                consumer_tag=spec.consumer_tag,
            )
        case ps.Basic.Cancel():
            return m.BasicCancel(
                consumer_tag=spec.consumer_tag,
                no_wait=spec.nowait,
            )
        case ps.Basic.CancelOk():
            return m.BasicCancelOk(
                consumer_tag=spec.consumer_tag,
            )
        case ps.Basic.Publish():
            return m.BasicPublish(
                exchange=spec.exchange,
                routing_key=spec.routing_key,
                mandatory=spec.mandatory,
                immediate=spec.immediate,
                properties=from_properties(properties),
                body=body,
            )
        case ps.Basic.Return():
            return m.BasicReturn(
                reply_code=spec.reply_code,
                reply_text=spec.reply_text,
                exchange=spec.exchange,
                routing_key=spec.routing_key,
                properties=from_properties(properties),
                body=body,
            )
        case ps.Basic.Ack():
            return m.BasicAck(
                delivery_tag=spec.delivery_tag,
                multiple=spec.multiple,
            )
        case ps.Basic.Nack():
            return m.BasicNack(
                delivery_tag=spec.delivery_tag,
                multiple=spec.multiple,
                requeue=spec.requeue,
            )
        case ps.Basic.Reject():
            return m.BasicReject(
                delivery_tag=spec.delivery_tag,
                requeue=spec.requeue,
            )
        case ps.Basic.Deliver():
            return m.BasicDeliver(
                consumer_tag=spec.consumer_tag,
                delivery_tag=spec.delivery_tag,
                redelivered=spec.redelivered,
                exchange=spec.exchange,
                routing_key=spec.routing_key,
                properties=from_properties(properties),
                body=body,
            )
        case ps.Basic.Get():
            return m.BasicGet(
                queue=spec.queue,
                no_ack=spec.no_ack,
            )
        case ps.Basic.GetOk():
            return m.BasicGetOk(
                delivery_tag=spec.delivery_tag,
                redelivered=spec.redelivered,
                exchange=spec.exchange,
                routing_key=spec.routing_key,
                message_count=spec.message_count,
                properties=from_properties(properties),
                body=body,
            )
        case ps.Basic.GetEmpty():
            return m.BasicGetEmpty()
        case _:
            return None


def encode(egress: Egress, frame_max: int) -> bytes:
    match egress:
        case Heartbeat():
            return cast(bytes, pf.Heartbeat().marshal())

        case MethodOnChannel(channel, partial_method):
            method, _ = mu.concretise(partial_method, DEFAULT_GEN_STATE)
            encoded = cast(bytes, pf.Method(channel, to_pika(method)).marshal())

            if not is_content_method(method):
                return encoded

            body = mf.assert_concrete(method.body)
            properties = to_pika_properties(mf.assert_concrete(method.properties))

            encoded += cast(bytes, pf.Header(channel, len(body), properties).marshal())
            limit = frame_max - ps.FRAME_HEADER_SIZE - ps.FRAME_END_SIZE

            for start in range(0, len(body), limit):
                encoded += cast(
                    bytes, pf.Body(channel, body[start : start + limit]).marshal()
                )

            return encoded


def validate_buffer_length(buffer: bytes, max_buffer: int) -> Undecodable | None:
    if len(buffer) > max_buffer:
        return Undecodable(f"buffer exceeds {max_buffer} bytes")
    return None


def decode_pika_frame(buffer: bytes) -> tuple[int, Any]:
    try:
        return cast(tuple[int, Any], pf.decode_frame(buffer))
    except (KeyError, struct.error) as bad:
        raise InvalidFrameError(f"undecodable frame: {bad!r}") from bad


def decode_content_method(
    buffer: bytes, offset: int, frame: Any, max_buffer: int
) -> tuple[Ingress | None, int]:
    consumed, header = decode_pika_frame(buffer[offset:])

    if header is None:
        return validate_buffer_length(buffer, max_buffer), len(buffer)

    if not isinstance(header, pf.Header):
        return Undecodable("expected a content header"), len(buffer)

    offset += consumed
    body = b""

    while len(body) < header.body_size:
        consumed, fragment = decode_pika_frame(buffer[offset:])

        if fragment is None:
            return validate_buffer_length(buffer, max_buffer), len(buffer)

        if not isinstance(fragment, pf.Body):
            return Undecodable("expected a content body"), len(buffer)

        body += fragment.fragment
        offset += consumed

    return decoded(frame, from_pika(frame.method, header.properties, body), offset)


def decoded(frame: Any, method: m.Method | None, offset: int) -> tuple[Ingress, int]:
    if method is None:
        return Undecodable(f"unsupported method: {frame.method.NAME}"), offset

    return MethodOnChannel(frame.channel_number, method), offset


def decode_method(
    buffer: bytes, offset: int, frame: Any, max_buffer: int
) -> tuple[Ingress | None, int]:
    if is_pika_content_method(frame.method):
        return decode_content_method(buffer, offset, frame, max_buffer)

    return decoded(frame, from_pika(frame.method), offset)


def decode_one(buffer: bytes, max_buffer: int) -> tuple[Ingress | None, int]:
    try:
        consumed, frame = decode_pika_frame(buffer)

        match frame:
            case None:
                return validate_buffer_length(buffer, max_buffer), len(buffer)
            case pf.ProtocolHeader():
                return ProtocolHeader(buffer[:consumed]), consumed
            case pf.Heartbeat():
                return Heartbeat(), consumed
            case pf.Method():
                return decode_method(buffer, consumed, frame, max_buffer)
            case _:
                reason = f"unexpected frame: {type(frame).__name__}"
                return Undecodable(reason), len(buffer)

    except InvalidFrameError as bad:
        return Undecodable(str(bad)), len(buffer)


def decode(buffer: bytes, max_buffer: int) -> tuple[tuple[Ingress, ...], bytes]:
    items: list[Ingress] = []
    rest = buffer

    while rest:
        item, consumed = decode_one(rest, max_buffer)

        if item is None:
            return tuple(items), rest

        items.append(item)
        rest = b"" if isinstance(item, Undecodable) else rest[consumed:]

    return tuple(items), rest
