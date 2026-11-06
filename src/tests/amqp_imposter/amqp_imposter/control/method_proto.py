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

from datetime import UTC, datetime
from decimal import Decimal

from ..amqp.transcript import TranscriptEvent, TranscriptLine
from ..imposter import ImposterError
from ..model import method_fields as mf
from ..model import methods as m
from .helpers import (
    concrete_map_or,
    concrete_or,
    cond_map,
    cond_value,
)
from .v1 import imposter_pb2 as pb


def to_proto(method: m.Method) -> pb.Method:
    match method:
        case m.ConnectionStart():
            return pb.Method(
                connection_start=pb.ConnectionStart(
                    version_major=concrete_or(method.version_major, 0),
                    version_minor=concrete_or(method.version_minor, 0),
                    mechanisms=concrete_or(method.mechanisms),
                    locales=concrete_or(method.locales),
                    publisher_confirms=concrete_or(method.publisher_confirms, False),
                    basic_nack=concrete_or(method.basic_nack, False),
                    consumer_cancel_notify=concrete_or(
                        method.consumer_cancel_notify, False
                    ),
                )
            )
        case m.ConnectionStartOk():
            return pb.Method(
                connection_start_ok=pb.ConnectionStartOk(
                    mechanism=concrete_or(method.mechanism, ""),
                    locale=concrete_or(method.locale, ""),
                    response=concrete_or(method.response, ""),
                )
            )
        case m.ConnectionTune():
            return pb.Method(
                connection_tune=pb.ConnectionTune(
                    channel_max=concrete_or(method.channel_max, 0),
                    frame_max=concrete_or(method.frame_max, 0),
                    heartbeat=concrete_or(method.heartbeat, 0),
                )
            )
        case m.ConnectionTuneOk():
            return pb.Method(
                connection_tune_ok=pb.ConnectionTuneOk(
                    channel_max=concrete_or(method.channel_max, 0),
                    frame_max=concrete_or(method.frame_max, 0),
                    heartbeat=concrete_or(method.heartbeat, 0),
                )
            )
        case m.ConnectionOpen():
            return pb.Method(
                connection_open=pb.ConnectionOpen(
                    vhost=concrete_or(method.vhost, ""),
                )
            )
        case m.ConnectionOpenOk():
            return pb.Method(connection_open_ok=pb.ConnectionOpenOk())
        case m.ConnectionClose():
            return pb.Method(
                connection_close=pb.ConnectionClose(
                    reply_code=concrete_or(method.reply_code, 0),
                    reply_text=concrete_or(method.reply_text, ""),
                    class_id=concrete_or(method.class_id, 0),
                    method_id=concrete_or(method.method_id, 0),
                )
            )
        case m.ConnectionCloseOk():
            return pb.Method(connection_close_ok=pb.ConnectionCloseOk())
        case m.ChannelOpen():
            return pb.Method(channel_open=pb.ChannelOpen())
        case m.ChannelOpenOk():
            return pb.Method(channel_open_ok=pb.ChannelOpenOk())
        case m.ChannelClose():
            return pb.Method(
                channel_close=pb.ChannelClose(
                    reply_code=concrete_or(method.reply_code, 0),
                    reply_text=concrete_or(method.reply_text, ""),
                    class_id=concrete_or(method.class_id, 0),
                    method_id=concrete_or(method.method_id, 0),
                )
            )
        case m.ChannelCloseOk():
            return pb.Method(channel_close_ok=pb.ChannelCloseOk())
        case m.ConfirmSelect():
            return pb.Method(
                confirm_select=pb.ConfirmSelect(
                    no_wait=concrete_or(method.no_wait, False),
                )
            )
        case m.ConfirmSelectOk():
            return pb.Method(confirm_select_ok=pb.ConfirmSelectOk())
        case m.ExchangeDeclare():
            return pb.Method(
                exchange_declare=pb.ExchangeDeclare(
                    exchange=concrete_or(method.exchange, ""),
                    exchange_type=concrete_or(method.exchange_type, ""),
                    passive=concrete_or(method.passive, False),
                    durable=concrete_or(method.durable, False),
                    auto_delete=concrete_or(method.auto_delete, False),
                    internal=concrete_or(method.internal, False),
                    no_wait=concrete_or(method.no_wait, False),
                    arguments=concrete_map_or(method.arguments, table_to_proto),
                )
            )
        case m.ExchangeDeclareOk():
            return pb.Method(exchange_declare_ok=pb.ExchangeDeclareOk())
        case m.ExchangeBind():
            return pb.Method(
                exchange_bind=pb.ExchangeBind(
                    destination=concrete_or(method.destination, ""),
                    source=concrete_or(method.source, ""),
                    routing_key=concrete_or(method.routing_key, ""),
                    no_wait=concrete_or(method.no_wait, False),
                    arguments=concrete_map_or(method.arguments, table_to_proto),
                )
            )
        case m.ExchangeBindOk():
            return pb.Method(exchange_bind_ok=pb.ExchangeBindOk())
        case m.QueueDeclare():
            return pb.Method(
                queue_declare=pb.QueueDeclare(
                    queue=concrete_or(method.queue, ""),
                    passive=concrete_or(method.passive, False),
                    durable=concrete_or(method.durable, False),
                    exclusive=concrete_or(method.exclusive, False),
                    auto_delete=concrete_or(method.auto_delete, False),
                    no_wait=concrete_or(method.no_wait, False),
                    arguments=concrete_map_or(method.arguments, table_to_proto),
                )
            )
        case m.QueueDeclareOk():
            return pb.Method(
                queue_declare_ok=pb.QueueDeclareOk(
                    queue=concrete_or(method.queue, ""),
                    message_count=concrete_or(method.message_count, 0),
                    consumer_count=concrete_or(method.consumer_count, 0),
                )
            )
        case m.QueueBind():
            return pb.Method(
                queue_bind=pb.QueueBind(
                    queue=concrete_or(method.queue, ""),
                    exchange=concrete_or(method.exchange, ""),
                    routing_key=concrete_or(method.routing_key, ""),
                    no_wait=concrete_or(method.no_wait, False),
                    arguments=concrete_map_or(method.arguments, table_to_proto),
                )
            )
        case m.QueueBindOk():
            return pb.Method(queue_bind_ok=pb.QueueBindOk())
        case m.BasicQos():
            return pb.Method(
                basic_qos=pb.BasicQos(
                    prefetch_count=concrete_or(method.prefetch_count, 0),
                    prefetch_size=concrete_or(method.prefetch_size, 0),
                    global_qos=concrete_or(method.global_qos, False),
                )
            )
        case m.BasicQosOk():
            return pb.Method(basic_qos_ok=pb.BasicQosOk())
        case m.BasicConsume():
            return pb.Method(
                basic_consume=pb.BasicConsume(
                    queue=concrete_or(method.queue, ""),
                    consumer_tag=concrete_or(method.consumer_tag, ""),
                    no_local=concrete_or(method.no_local, False),
                    no_ack=concrete_or(method.no_ack, False),
                    no_wait=concrete_or(method.no_wait, False),
                    exclusive=concrete_or(method.exclusive, False),
                    arguments=concrete_map_or(method.arguments, table_to_proto),
                )
            )
        case m.BasicConsumeOk():
            return pb.Method(
                basic_consume_ok=pb.BasicConsumeOk(
                    consumer_tag=concrete_or(method.consumer_tag, ""),
                )
            )
        case m.BasicCancel():
            return pb.Method(
                basic_cancel=pb.BasicCancel(
                    consumer_tag=concrete_or(method.consumer_tag, ""),
                    no_wait=concrete_or(method.no_wait, False),
                )
            )
        case m.BasicCancelOk():
            return pb.Method(
                basic_cancel_ok=pb.BasicCancelOk(
                    consumer_tag=concrete_or(method.consumer_tag, ""),
                )
            )
        case m.BasicPublish():
            return pb.Method(
                basic_publish=pb.BasicPublish(
                    exchange=concrete_or(method.exchange, ""),
                    routing_key=concrete_or(method.routing_key, ""),
                    mandatory=concrete_or(method.mandatory, False),
                    immediate=concrete_or(method.immediate, False),
                    properties=concrete_map_or(method.properties, properties_to_proto),
                    body=concrete_or(method.body, b""),
                )
            )
        case m.BasicReturn():
            return pb.Method(
                basic_return=pb.BasicReturn(
                    reply_code=concrete_or(method.reply_code, 0),
                    reply_text=concrete_or(method.reply_text, ""),
                    exchange=concrete_or(method.exchange, ""),
                    routing_key=concrete_or(method.routing_key, ""),
                    properties=concrete_map_or(method.properties, properties_to_proto),
                    body=concrete_or(method.body, b""),
                )
            )
        case m.BasicAck():
            return pb.Method(
                basic_ack=pb.BasicAck(
                    delivery_tag=concrete_or(method.delivery_tag, 0),
                    multiple=concrete_or(method.multiple, False),
                )
            )
        case m.BasicNack():
            return pb.Method(
                basic_nack=pb.BasicNack(
                    delivery_tag=concrete_or(method.delivery_tag, 0),
                    multiple=concrete_or(method.multiple, False),
                    requeue=concrete_or(method.requeue, False),
                )
            )
        case m.BasicReject():
            return pb.Method(
                basic_reject=pb.BasicReject(
                    delivery_tag=concrete_or(method.delivery_tag, 0),
                    requeue=concrete_or(method.requeue, False),
                )
            )
        case m.BasicDeliver():
            return pb.Method(
                basic_deliver=pb.BasicDeliver(
                    consumer_tag=concrete_or(method.consumer_tag, ""),
                    delivery_tag=concrete_or(method.delivery_tag, 0),
                    redelivered=concrete_or(method.redelivered, False),
                    exchange=concrete_or(method.exchange, ""),
                    routing_key=concrete_or(method.routing_key, ""),
                    properties=concrete_map_or(method.properties, properties_to_proto),
                    body=concrete_or(method.body, b""),
                )
            )
        case m.BasicGet():
            return pb.Method(
                basic_get=pb.BasicGet(
                    queue=concrete_or(method.queue, ""),
                    no_ack=concrete_or(method.no_ack, False),
                )
            )
        case m.BasicGetOk():
            return pb.Method(
                basic_get_ok=pb.BasicGetOk(
                    delivery_tag=concrete_or(method.delivery_tag, 0),
                    redelivered=concrete_or(method.redelivered, False),
                    exchange=concrete_or(method.exchange, ""),
                    routing_key=concrete_or(method.routing_key, ""),
                    message_count=concrete_or(method.message_count, 0),
                    properties=concrete_map_or(method.properties, properties_to_proto),
                    body=concrete_or(method.body, b""),
                )
            )
        case m.BasicGetEmpty():
            return pb.Method(basic_get_empty=pb.BasicGetEmpty())


def from_proto(message: pb.Method) -> m.Method:
    match message.WhichOneof("kind"):
        case None:
            raise ImposterError("no method given")
        case "connection_start":
            return m.ConnectionStart(
                version_major=message.connection_start.version_major,
                version_minor=message.connection_start.version_minor,
                mechanisms=tuple(message.connection_start.mechanisms),
                locales=tuple(message.connection_start.locales),
                publisher_confirms=message.connection_start.publisher_confirms,
                basic_nack=message.connection_start.basic_nack,
                consumer_cancel_notify=message.connection_start.consumer_cancel_notify,
            )
        case "connection_start_ok":
            return m.ConnectionStartOk(
                mechanism=message.connection_start_ok.mechanism,
                locale=message.connection_start_ok.locale,
                response=message.connection_start_ok.response,
            )
        case "connection_tune":
            return m.ConnectionTune(
                channel_max=message.connection_tune.channel_max,
                frame_max=message.connection_tune.frame_max,
                heartbeat=message.connection_tune.heartbeat,
            )
        case "connection_tune_ok":
            return m.ConnectionTuneOk(
                channel_max=message.connection_tune_ok.channel_max,
                frame_max=message.connection_tune_ok.frame_max,
                heartbeat=message.connection_tune_ok.heartbeat,
            )
        case "connection_open":
            return m.ConnectionOpen(
                vhost=message.connection_open.vhost,
            )
        case "connection_open_ok":
            return m.ConnectionOpenOk()
        case "connection_close":
            return m.ConnectionClose(
                reply_code=message.connection_close.reply_code,
                reply_text=message.connection_close.reply_text,
                class_id=message.connection_close.class_id,
                method_id=message.connection_close.method_id,
            )
        case "connection_close_ok":
            return m.ConnectionCloseOk()
        case "channel_open":
            return m.ChannelOpen()
        case "channel_open_ok":
            return m.ChannelOpenOk()
        case "channel_close":
            return m.ChannelClose(
                reply_code=message.channel_close.reply_code,
                reply_text=message.channel_close.reply_text,
                class_id=message.channel_close.class_id,
                method_id=message.channel_close.method_id,
            )
        case "channel_close_ok":
            return m.ChannelCloseOk()
        case "confirm_select":
            return m.ConfirmSelect(
                no_wait=message.confirm_select.no_wait,
            )
        case "confirm_select_ok":
            return m.ConfirmSelectOk()
        case "exchange_declare":
            return m.ExchangeDeclare(
                exchange=message.exchange_declare.exchange,
                exchange_type=message.exchange_declare.exchange_type,
                passive=message.exchange_declare.passive,
                durable=message.exchange_declare.durable,
                auto_delete=message.exchange_declare.auto_delete,
                internal=message.exchange_declare.internal,
                no_wait=message.exchange_declare.no_wait,
                arguments=cond_map(
                    message.exchange_declare.HasField("arguments"),
                    message.exchange_declare.arguments,
                    table_from_proto,
                ),
            )
        case "exchange_declare_ok":
            return m.ExchangeDeclareOk()
        case "exchange_bind":
            return m.ExchangeBind(
                destination=message.exchange_bind.destination,
                source=message.exchange_bind.source,
                routing_key=message.exchange_bind.routing_key,
                no_wait=message.exchange_bind.no_wait,
                arguments=cond_map(
                    message.exchange_bind.HasField("arguments"),
                    message.exchange_bind.arguments,
                    table_from_proto,
                ),
            )
        case "exchange_bind_ok":
            return m.ExchangeBindOk()
        case "queue_declare":
            return m.QueueDeclare(
                queue=message.queue_declare.queue,
                passive=message.queue_declare.passive,
                durable=message.queue_declare.durable,
                exclusive=message.queue_declare.exclusive,
                auto_delete=message.queue_declare.auto_delete,
                no_wait=message.queue_declare.no_wait,
                arguments=cond_map(
                    message.queue_declare.HasField("arguments"),
                    message.queue_declare.arguments,
                    table_from_proto,
                ),
            )
        case "queue_declare_ok":
            return m.QueueDeclareOk(
                queue=message.queue_declare_ok.queue,
                message_count=message.queue_declare_ok.message_count,
                consumer_count=message.queue_declare_ok.consumer_count,
            )
        case "queue_bind":
            return m.QueueBind(
                queue=message.queue_bind.queue,
                exchange=message.queue_bind.exchange,
                routing_key=message.queue_bind.routing_key,
                no_wait=message.queue_bind.no_wait,
                arguments=cond_map(
                    message.queue_bind.HasField("arguments"),
                    message.queue_bind.arguments,
                    table_from_proto,
                ),
            )
        case "queue_bind_ok":
            return m.QueueBindOk()
        case "basic_qos":
            return m.BasicQos(
                prefetch_count=message.basic_qos.prefetch_count,
                prefetch_size=message.basic_qos.prefetch_size,
                global_qos=message.basic_qos.global_qos,
            )
        case "basic_qos_ok":
            return m.BasicQosOk()
        case "basic_consume":
            return m.BasicConsume(
                queue=message.basic_consume.queue,
                consumer_tag=message.basic_consume.consumer_tag,
                no_local=message.basic_consume.no_local,
                no_ack=message.basic_consume.no_ack,
                no_wait=message.basic_consume.no_wait,
                exclusive=message.basic_consume.exclusive,
                arguments=cond_map(
                    message.basic_consume.HasField("arguments"),
                    message.basic_consume.arguments,
                    table_from_proto,
                ),
            )
        case "basic_consume_ok":
            return m.BasicConsumeOk(
                consumer_tag=message.basic_consume_ok.consumer_tag,
            )
        case "basic_cancel":
            return m.BasicCancel(
                consumer_tag=message.basic_cancel.consumer_tag,
                no_wait=message.basic_cancel.no_wait,
            )
        case "basic_cancel_ok":
            return m.BasicCancelOk(
                consumer_tag=message.basic_cancel_ok.consumer_tag,
            )
        case "basic_publish":
            return m.BasicPublish(
                exchange=message.basic_publish.exchange,
                routing_key=message.basic_publish.routing_key,
                mandatory=message.basic_publish.mandatory,
                immediate=message.basic_publish.immediate,
                properties=cond_map(
                    message.basic_publish.HasField("properties"),
                    message.basic_publish.properties,
                    properties_from_proto,
                ),
                body=message.basic_publish.body,
            )
        case "basic_return":
            return m.BasicReturn(
                reply_code=message.basic_return.reply_code,
                reply_text=message.basic_return.reply_text,
                exchange=message.basic_return.exchange,
                routing_key=message.basic_return.routing_key,
                properties=cond_map(
                    message.basic_return.HasField("properties"),
                    message.basic_return.properties,
                    properties_from_proto,
                ),
                body=message.basic_return.body,
            )
        case "basic_ack":
            return m.BasicAck(
                delivery_tag=message.basic_ack.delivery_tag,
                multiple=message.basic_ack.multiple,
            )
        case "basic_nack":
            return m.BasicNack(
                delivery_tag=message.basic_nack.delivery_tag,
                multiple=message.basic_nack.multiple,
                requeue=message.basic_nack.requeue,
            )
        case "basic_reject":
            return m.BasicReject(
                delivery_tag=message.basic_reject.delivery_tag,
                requeue=message.basic_reject.requeue,
            )
        case "basic_deliver":
            return m.BasicDeliver(
                consumer_tag=message.basic_deliver.consumer_tag,
                delivery_tag=message.basic_deliver.delivery_tag,
                redelivered=message.basic_deliver.redelivered,
                exchange=message.basic_deliver.exchange,
                routing_key=message.basic_deliver.routing_key,
                properties=cond_map(
                    message.basic_deliver.HasField("properties"),
                    message.basic_deliver.properties,
                    properties_from_proto,
                ),
                body=message.basic_deliver.body,
            )
        case "basic_get":
            return m.BasicGet(
                queue=message.basic_get.queue,
                no_ack=message.basic_get.no_ack,
            )
        case "basic_get_ok":
            return m.BasicGetOk(
                delivery_tag=message.basic_get_ok.delivery_tag,
                redelivered=message.basic_get_ok.redelivered,
                exchange=message.basic_get_ok.exchange,
                routing_key=message.basic_get_ok.routing_key,
                message_count=message.basic_get_ok.message_count,
                properties=cond_map(
                    message.basic_get_ok.HasField("properties"),
                    message.basic_get_ok.properties,
                    properties_from_proto,
                ),
                body=message.basic_get_ok.body,
            )
        case "basic_get_empty":
            return m.BasicGetEmpty()


def value_to_proto(value: mf.TableValue) -> pb.AmqpValue:
    message = pb.AmqpValue()

    match value:
        case bool():
            message.boolean = value
        case int():
            message.long = value
        case str():
            message.string = value
        case bytes():
            message.bytes = value
        case Decimal():
            message.decimal = str(value)
        case datetime():
            message.timestamp.FromDatetime(value)
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
            return message.long
        case "string":
            return message.string
        case "bytes":
            return message.bytes
        case "decimal":
            return Decimal(message.decimal)
        case "timestamp":
            return message.timestamp.ToDatetime(tzinfo=UTC)
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
        (key, value_from_proto(value)) for key, value in message.entries.items()
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
        content_type=cond_value(message.HasField("content_type"), message.content_type),
        content_encoding=cond_value(
            message.HasField("content_encoding"), message.content_encoding
        ),
        headers=cond_map(
            message.HasField("headers"), message.headers, table_from_proto
        ),
        delivery_mode=cond_value(
            message.HasField("delivery_mode"), message.delivery_mode
        ),
        priority=cond_value(message.HasField("priority"), message.priority),
        correlation_id=cond_value(
            message.HasField("correlation_id"), message.correlation_id
        ),
        reply_to=cond_value(message.HasField("reply_to"), message.reply_to),
        expiration=cond_value(message.HasField("expiration"), message.expiration),
        message_id=cond_value(message.HasField("message_id"), message.message_id),
        timestamp=cond_value(message.HasField("timestamp"), message.timestamp),
        type=cond_value(message.HasField("type"), message.type),
        user_id=cond_value(message.HasField("user_id"), message.user_id),
        app_id=cond_value(message.HasField("app_id"), message.app_id),
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
