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

from collections.abc import Callable, Mapping
from dataclasses import dataclass
from typing import Any, get_args, get_type_hints

from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import Message

from ..amqp.transcript import TranscriptEvent, TranscriptLine
from ..imposter import ImposterError
from ..model import domains as d
from ..model import method_fields as mf
from ..model import methods as m
from .helpers import cond_map
from .v1 import imposter_pb2 as pb


def field_to_proto(field: FieldDescriptor, value: Any) -> Any:
    match field.message_type:
        case pb.StringList.DESCRIPTOR:
            return pb.StringList(items=value)
        case pb.AmqpTable.DESCRIPTOR:
            return table_to_proto(value)
        case pb.MessageProperties.DESCRIPTOR:
            return properties_to_proto(value)
        case _:
            return value


def field_from_proto(field: FieldDescriptor, value: Any, scalar_type: Any) -> Any:
    match field.message_type:
        case pb.StringList.DESCRIPTOR:
            return tuple(value.items)
        case pb.AmqpTable.DESCRIPTOR:
            return table_from_proto(value)
        case pb.MessageProperties.DESCRIPTOR:
            return properties_from_proto(value)
        case _ if field.is_repeated:
            return tuple(value)
        case _:
            return scalar_type(value)


def fields_to_proto(method: m.Method, message_type: Any) -> Any:
    fields = message_type.DESCRIPTOR.fields_by_name

    return message_type(
        **{
            name: field_to_proto(fields[name], value)
            for name, value in m.get_fields(method).items()
            if mf.is_concrete(value)
        }
    )


def fields_from_proto[M: m.Method](kind: type[M], message: Any) -> M:
    types = {name: get_args(hint)[0] for name, hint in get_type_hints(kind).items()}

    try:
        return kind(
            **{
                field.name: field_from_proto(
                    field, getattr(message, field.name), types[field.name]
                )
                for field in message.DESCRIPTOR.fields
                if not field.has_presence or message.HasField(field.name)
            }
        )
    except d.BadDomain as bad:
        raise ImposterError(str(bad)) from bad


@dataclass(frozen=True, slots=True)
class MethodOneof[C: Message, M: m.Method]:
    container: Callable[..., C]
    oneof: str
    oneof_fields_by_method_kind: Mapping[type[M], tuple[str, type[Message]]]
    method_kinds_by_field_name: Mapping[str, type[M]]

    def to_proto(self, method: M) -> C:
        field_name, message_type = self.oneof_fields_by_method_kind[type(method)]

        return self.container(**{field_name: fields_to_proto(method, message_type)})

    def from_proto(self, message: C) -> M:
        match message.WhichOneof(self.oneof):
            case None:
                raise ImposterError(f"empty {message.DESCRIPTOR.name}")
            case field_name:
                return fields_from_proto(
                    self.method_kinds_by_field_name[field_name],
                    getattr(message, field_name),
                )


def method_oneof[C: Message, M: m.Method](
    container: type[C], oneof: str, kinds: tuple[type[M], ...], suffix: str
) -> MethodOneof[C, M]:
    method_kinds_by_message_name = {kind.__name__ + suffix: kind for kind in kinds}
    # for example, {
    #   "ConnectionClosePattern":  "ConnectionClose"
    #   "ConnectionCloseSelector": "ConnectionClose"
    #   ...
    # }

    method_kinds_by_field_name = {
        field.name: method_kinds_by_message_name[field.message_type.name]
        for field in container.DESCRIPTOR.oneofs_by_name[oneof].fields
        if field.message_type is not None
    }
    # for example, {
    #   "connection_close": "ConnectionClose"
    #   "basic_publish":    "BasicPublish"
    #   ...
    # }

    prototype = container()

    return MethodOneof(
        container,
        oneof,
        {
            kind: (field_name, type(getattr(prototype, field_name)))
            for field_name, kind in method_kinds_by_field_name.items()
        },
        method_kinds_by_field_name,
    )


METHODS = method_oneof(pb.Method, "kind", m.METHOD_KINDS, "")


def to_proto(method: m.Method) -> pb.Method:
    return METHODS.to_proto(method)


def from_proto(message: pb.Method) -> m.Method:
    return METHODS.from_proto(message)


def value_to_proto(value: mf.TableValue) -> pb.AmqpValue:
    message = pb.AmqpValue()

    match value:
        case bool():
            message.boolean = value
        case d.SignedLongLong():
            message.long = value
        case d.Timestamp():
            message.timestamp.seconds = value
        case str():
            message.string = value
        case bytes():
            message.bytes = value
        case d.DecimalValue():
            message.decimal = str(value)
        case mf.Void():
            message.void.SetInParent()
        case frozendict():
            message.table.CopyFrom(table_to_proto(value))
        case tuple():
            message.array.SetInParent()
            message.array.items.extend(value_to_proto(item) for item in value)

    return message


def value_from_proto(message: pb.AmqpValue) -> mf.TableValue:
    match message.WhichOneof("value"):
        case None:
            raise ImposterError("no table value given")
        case "boolean":
            return message.boolean
        case "long":
            return d.SignedLongLong(message.long)
        case "string":
            return message.string
        case "bytes":
            return message.bytes
        case "decimal":
            return d.DecimalValue(message.decimal)
        case "timestamp" if message.timestamp.nanos:
            raise ImposterError(f"timestamp has nanos: {message.timestamp.nanos}")
        case "timestamp":
            return d.Timestamp(message.timestamp.seconds)
        case "void":
            return mf.Void()
        case "table":
            return table_from_proto(message.table)
        case _:
            return tuple(value_from_proto(item) for item in message.array.items)


def table_to_proto(table: mf.Table) -> pb.AmqpTable:
    message = pb.AmqpTable()

    for key, value in table.items():
        message.entries[key].CopyFrom(value_to_proto(value))

    return message


def table_from_proto(message: pb.AmqpTable) -> mf.Table:
    return frozendict(
        (d.ShortStr(key), value_from_proto(value))
        for key, value in message.entries.items()
    )


def properties_to_proto(properties: mf.MessageProperties) -> pb.MessageProperties:
    return pb.MessageProperties(
        content_type=properties.content_type,
        content_encoding=properties.content_encoding,
        headers=None
        if properties.headers is None
        else table_to_proto(properties.headers),
        delivery_mode=properties.delivery_mode,
        priority=properties.priority,
        correlation_id=properties.correlation_id,
        reply_to=properties.reply_to,
        expiration=properties.expiration,
        message_id=properties.message_id,
        timestamp=properties.timestamp,
        type=properties.type,
        user_id=properties.user_id,
        app_id=properties.app_id,
    )


def properties_from_proto(message: pb.MessageProperties) -> mf.MessageProperties:
    return mf.MessageProperties(
        content_type=cond_map(
            message.HasField("content_type"), message.content_type, d.ShortStr
        ),
        content_encoding=cond_map(
            message.HasField("content_encoding"), message.content_encoding, d.ShortStr
        ),
        headers=cond_map(
            message.HasField("headers"), message.headers, table_from_proto
        ),
        delivery_mode=cond_map(
            message.HasField("delivery_mode"), message.delivery_mode, d.Octet
        ),
        priority=cond_map(message.HasField("priority"), message.priority, d.Octet),
        correlation_id=cond_map(
            message.HasField("correlation_id"), message.correlation_id, d.ShortStr
        ),
        reply_to=cond_map(message.HasField("reply_to"), message.reply_to, d.ShortStr),
        expiration=cond_map(
            message.HasField("expiration"), message.expiration, d.ShortStr
        ),
        message_id=cond_map(
            message.HasField("message_id"), message.message_id, d.ShortStr
        ),
        timestamp=cond_map(
            message.HasField("timestamp"), message.timestamp, d.Timestamp
        ),
        type=cond_map(message.HasField("type"), message.type, d.ShortStr),
        user_id=cond_map(message.HasField("user_id"), message.user_id, d.ShortStr),
        app_id=cond_map(message.HasField("app_id"), message.app_id, d.ShortStr),
    )


def transcript_event_to_proto(event: TranscriptEvent) -> pb.TranscriptEvent.ValueType:
    match event:
        case TranscriptEvent.SESSION_STARTED:
            return pb.TRANSCRIPT_EVENT_SESSION_STARTED
        case TranscriptEvent.TCP_CONNECTED:
            return pb.TRANSCRIPT_EVENT_TCP_CONNECTED
        case TranscriptEvent.TCP_REJECTED:
            return pb.TRANSCRIPT_EVENT_TCP_REJECTED
        case TranscriptEvent.TCP_DOWN:
            return pb.TRANSCRIPT_EVENT_TCP_DOWN
        case TranscriptEvent.IDLE_TIMEOUT:
            return pb.TRANSCRIPT_EVENT_IDLE_TIMEOUT
        case TranscriptEvent.PROTOCOL_HEADER:
            return pb.TRANSCRIPT_EVENT_PROTOCOL_HEADER
        case TranscriptEvent.FRAME_MALFORMED:
            return pb.TRANSCRIPT_EVENT_FRAME_MALFORMED
        case TranscriptEvent.FRAME_HEARTBEAT:
            return pb.TRANSCRIPT_EVENT_FRAME_HEARTBEAT
        case TranscriptEvent.METHOD_INGRESS:
            return pb.TRANSCRIPT_EVENT_METHOD_INGRESS
        case TranscriptEvent.METHOD_EGRESS:
            return pb.TRANSCRIPT_EVENT_METHOD_EGRESS
        case TranscriptEvent.METHOD_UNREPRESENTABLE:
            return pb.TRANSCRIPT_EVENT_METHOD_UNREPRESENTABLE


def line_to_proto(line: TranscriptLine) -> pb.TranscriptLine:
    message = pb.TranscriptLine(
        index=line.index,
        micros=line.micros,
        epoch_micros=line.epoch_micros,
        event=transcript_event_to_proto(line.event),
        detail=line.detail,
        wire_bytes=line.wire_bytes,
    )

    if line.method is not None:
        message.method.CopyFrom(to_proto(line.method))

    return message
