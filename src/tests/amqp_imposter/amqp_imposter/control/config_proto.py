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

from collections.abc import Iterable, Sequence

from ..amqp.session import DEFAULT_SESSION_CONFIG, SessionConfig
from ..amqp.transcript import HARD_MAX_TRANSCRIPT_LINES
from ..helpers.nonempty_tuple import NonEmptyTuple
from ..imposter import ImposterError
from ..model import constants as c
from ..model import method_fields as mf
from ..model import methods as m
from ..model.method_scripts import Received, ScriptRule, Sent
from ..model.method_weights import BASE_WEIGHT, BadWeight, WeightedPattern
from ..model.negotiation_options import (
    DEFAULT_NEGOTIATION_OPTIONS,
    NegotiationOptions,
)
from .helpers import concrete_or, cond_exact_field
from .v1 import imposter_pb2 as pb


def string_list_to_proto(value: mf.Exact[tuple[str, ...]]) -> pb.StringList | None:
    if (items := concrete_or(value, None)) is not None:
        return pb.StringList(items=items)
    else:
        return None


def validated_nonempty_tuple(values: Iterable[str], name: str) -> NonEmptyTuple[str]:
    match list(values):
        case [head, *rest]:
            return (head, *rest)
        case _:
            raise ImposterError(f"{name} cannot be empty")


def to_proto(config: SessionConfig) -> pb.SessionConfig:
    return pb.SessionConfig(
        seed=config.seed,
        negotiation_options=negotiation_to_proto(config.negotiation_options),
        weights=[weighted_to_proto(one) for one in config.weights],
        idle_weights=[weighted_to_proto(one) for one in config.idle_weights],
        script=[script_rule_to_proto(one) for one in config.script],
        stop_weight=config.stop_weight,
        max_sends=config.max_sends,
        idle_millis=config.idle_millis,
        max_buffer=config.max_buffer,
        max_transcript_lines=config.max_transcript_lines,
    )


def weights_from_proto(
    messages: Sequence[pb.Weighted], default: tuple[WeightedPattern, ...]
) -> tuple[WeightedPattern, ...]:
    if not messages:
        return default

    return tuple(weighted_from_proto(one) for one in messages)


def scripts_from_proto(messages: Sequence[pb.Scripted]) -> tuple[ScriptRule, ...]:
    return tuple(script_rule_from_proto(one) for one in messages)


def from_proto(message: pb.SessionConfig) -> SessionConfig:
    default = DEFAULT_SESSION_CONFIG

    return validated_config(
        SessionConfig(
            seed=message.seed if message.HasField("seed") else default.seed,
            negotiation_options=negotiation_from_proto(message.negotiation_options)
            if message.HasField("negotiation_options")
            else default.negotiation_options,
            weights=weights_from_proto(message.weights, default.weights),
            idle_weights=weights_from_proto(message.idle_weights, default.idle_weights),
            script=scripts_from_proto(message.script),
            stop_weight=message.stop_weight
            if message.HasField("stop_weight")
            else default.stop_weight,
            max_sends=message.max_sends
            if message.HasField("max_sends")
            else default.max_sends,
            idle_millis=message.idle_millis
            if message.HasField("idle_millis")
            else default.idle_millis,
            max_buffer=message.max_buffer
            if message.HasField("max_buffer")
            else default.max_buffer,
            max_transcript_lines=message.max_transcript_lines
            if message.HasField("max_transcript_lines")
            else default.max_transcript_lines,
        )
    )


def validated_config(config: SessionConfig) -> SessionConfig:
    if config.stop_weight <= 0:
        raise ImposterError(
            f"stop_weight must be positive: {config.stop_weight}",
        )
    if config.max_sends <= 0:
        raise ImposterError(
            f"max_sends must be positive: {config.max_sends}",
        )
    if config.idle_millis <= 0:
        raise ImposterError(
            f"idle_millis must be positive: {config.idle_millis}",
        )
    if config.max_buffer <= 0:
        raise ImposterError(
            f"max_buffer must be positive: {config.max_buffer}",
        )
    if not 0 < config.max_transcript_lines <= HARD_MAX_TRANSCRIPT_LINES:
        raise ImposterError(
            f"max_transcript_lines out of range: {config.max_transcript_lines}",
        )
    return config


def negotiation_to_proto(options: NegotiationOptions) -> pb.NegotiationOptions:
    return pb.NegotiationOptions(
        mechanisms=pb.StringList(items=options.mechanisms),
        locales=pb.StringList(items=options.locales),
        publisher_confirms=options.publisher_confirms,
        basic_nack=options.basic_nack,
        consumer_cancel_notify=options.consumer_cancel_notify,
        channel_max=options.channel_max,
        frame_max=options.frame_max,
        heartbeat=options.heartbeat,
    )


def negotiation_from_proto(message: pb.NegotiationOptions) -> NegotiationOptions:
    default = DEFAULT_NEGOTIATION_OPTIONS

    return validated_negotiation_options(
        NegotiationOptions(
            mechanisms=validated_nonempty_tuple(message.mechanisms.items, "mechanisms")
            if message.HasField("mechanisms")
            else default.mechanisms,
            locales=validated_nonempty_tuple(message.locales.items, "locales")
            if message.HasField("locales")
            else default.locales,
            publisher_confirms=message.publisher_confirms
            if message.HasField("publisher_confirms")
            else default.publisher_confirms,
            basic_nack=message.basic_nack
            if message.HasField("basic_nack")
            else default.basic_nack,
            consumer_cancel_notify=message.consumer_cancel_notify
            if message.HasField("consumer_cancel_notify")
            else default.consumer_cancel_notify,
            channel_max=message.channel_max
            if message.HasField("channel_max")
            else default.channel_max,
            frame_max=message.frame_max
            if message.HasField("frame_max")
            else default.frame_max,
            heartbeat=message.heartbeat
            if message.HasField("heartbeat")
            else default.heartbeat,
        )
    )


def validated_negotiation_options(options: NegotiationOptions) -> NegotiationOptions:
    if not 0 <= options.channel_max <= c.CHANNEL_MAX_CEIL:
        raise ImposterError(
            f"channel_max out of range: {options.channel_max}",
        )
    if not c.FRAME_MIN_FLOOR <= options.frame_max <= c.FRAME_MAX_CEIL:
        raise ImposterError(
            f"frame_max out of range: {options.frame_max}",
        )
    if not 0 <= options.heartbeat <= c.HEARTBEAT_MAX:
        raise ImposterError(
            f"heartbeat out of range: {options.heartbeat}",
        )
    return options


def weighted_to_proto(weighted: WeightedPattern) -> pb.Weighted:
    return pb.Weighted(
        pattern=pattern_to_proto(weighted.pattern), weight=weighted.weight
    )


def weighted_from_proto(message: pb.Weighted) -> WeightedPattern:
    weight = message.weight if message.HasField("weight") else BASE_WEIGHT

    try:
        return WeightedPattern(pattern_from_proto(message.pattern), weight)
    except (BadWeight, m.BadPattern) as bad:
        raise ImposterError(str(bad)) from bad


def script_rule_to_proto(rule: ScriptRule) -> pb.Scripted:
    then_send = pattern_to_proto(rule.then_send)

    match rule.when:
        case Received(method):
            return pb.Scripted(received=pattern_to_proto(method), then_send=then_send)
        case Sent(method):
            return pb.Scripted(sent=pattern_to_proto(method), then_send=then_send)


def when_from_proto(message: pb.Scripted) -> Received | Sent:
    match message.WhichOneof("when"):
        case None:
            raise ImposterError("no when given")
        case "received":
            return Received(pattern_from_proto(message.received))
        case "sent":
            return Sent(pattern_from_proto(message.sent))


def script_rule_from_proto(message: pb.Scripted) -> ScriptRule:
    when = when_from_proto(message)
    then_send = pattern_from_proto(message.then_send)

    try:
        return ScriptRule(when, then_send)
    except m.BadPattern as bad:
        raise ImposterError(str(bad)) from bad


def pattern_to_proto(pattern: m.Method) -> pb.Pattern:
    match pattern:
        case m.ConnectionStart():
            return pb.Pattern(
                connection_start=pb.ConnectionStartPattern(
                    version_major=concrete_or(pattern.version_major),
                    version_minor=concrete_or(pattern.version_minor),
                    mechanisms=string_list_to_proto(pattern.mechanisms),
                    locales=string_list_to_proto(pattern.locales),
                    publisher_confirms=concrete_or(pattern.publisher_confirms),
                    basic_nack=concrete_or(pattern.basic_nack),
                    consumer_cancel_notify=concrete_or(pattern.consumer_cancel_notify),
                ),
            )
        case m.ConnectionStartOk():
            return pb.Pattern(
                connection_start_ok=pb.ConnectionStartOkPattern(
                    mechanism=concrete_or(pattern.mechanism),
                    locale=concrete_or(pattern.locale),
                ),
            )
        case m.ConnectionTune():
            return pb.Pattern(
                connection_tune=pb.ConnectionTunePattern(
                    channel_max=concrete_or(pattern.channel_max),
                    frame_max=concrete_or(pattern.frame_max),
                    heartbeat=concrete_or(pattern.heartbeat),
                ),
            )
        case m.ConnectionTuneOk():
            return pb.Pattern(
                connection_tune_ok=pb.ConnectionTuneOkPattern(
                    channel_max=concrete_or(pattern.channel_max),
                    frame_max=concrete_or(pattern.frame_max),
                    heartbeat=concrete_or(pattern.heartbeat),
                ),
            )
        case m.ConnectionOpen():
            return pb.Pattern(connection_open=pb.ConnectionOpenPattern())
        case m.ConnectionOpenOk():
            return pb.Pattern(connection_open_ok=pb.ConnectionOpenOkPattern())
        case m.ConnectionClose():
            return pb.Pattern(
                connection_close=pb.ConnectionClosePattern(
                    reply_code=concrete_or(pattern.reply_code),
                    reply_text=concrete_or(pattern.reply_text),
                    class_id=concrete_or(pattern.class_id),
                    method_id=concrete_or(pattern.method_id),
                ),
            )
        case m.ConnectionCloseOk():
            return pb.Pattern(connection_close_ok=pb.ConnectionCloseOkPattern())
        case m.ChannelOpen():
            return pb.Pattern(channel_open=pb.ChannelOpenPattern())
        case m.ChannelOpenOk():
            return pb.Pattern(channel_open_ok=pb.ChannelOpenOkPattern())
        case m.ChannelClose():
            return pb.Pattern(
                channel_close=pb.ChannelClosePattern(
                    reply_code=concrete_or(pattern.reply_code),
                    reply_text=concrete_or(pattern.reply_text),
                    class_id=concrete_or(pattern.class_id),
                    method_id=concrete_or(pattern.method_id),
                ),
            )
        case m.ChannelCloseOk():
            return pb.Pattern(channel_close_ok=pb.ChannelCloseOkPattern())
        case m.ConfirmSelect():
            return pb.Pattern(confirm_select=pb.ConfirmSelectPattern())
        case m.ConfirmSelectOk():
            return pb.Pattern(confirm_select_ok=pb.ConfirmSelectOkPattern())
        case m.ExchangeDeclare():
            return pb.Pattern(
                exchange_declare=pb.ExchangeDeclarePattern(
                    exchange=concrete_or(pattern.exchange),
                ),
            )
        case m.ExchangeDeclareOk():
            return pb.Pattern(exchange_declare_ok=pb.ExchangeDeclareOkPattern())
        case m.ExchangeBind():
            return pb.Pattern(exchange_bind=pb.ExchangeBindPattern())
        case m.ExchangeBindOk():
            return pb.Pattern(exchange_bind_ok=pb.ExchangeBindOkPattern())
        case m.QueueDeclare():
            return pb.Pattern(queue_declare=pb.QueueDeclarePattern())
        case m.QueueDeclareOk():
            return pb.Pattern(queue_declare_ok=pb.QueueDeclareOkPattern())
        case m.QueueBind():
            return pb.Pattern(queue_bind=pb.QueueBindPattern())
        case m.QueueBindOk():
            return pb.Pattern(queue_bind_ok=pb.QueueBindOkPattern())
        case m.BasicQos():
            return pb.Pattern(basic_qos=pb.BasicQosPattern())
        case m.BasicQosOk():
            return pb.Pattern(basic_qos_ok=pb.BasicQosOkPattern())
        case m.BasicConsume():
            return pb.Pattern(basic_consume=pb.BasicConsumePattern())
        case m.BasicConsumeOk():
            return pb.Pattern(basic_consume_ok=pb.BasicConsumeOkPattern())
        case m.BasicCancel():
            return pb.Pattern(
                basic_cancel=pb.BasicCancelPattern(
                    consumer_tag=concrete_or(pattern.consumer_tag),
                ),
            )
        case m.BasicCancelOk():
            return pb.Pattern(
                basic_cancel_ok=pb.BasicCancelOkPattern(
                    consumer_tag=concrete_or(pattern.consumer_tag),
                ),
            )
        case m.BasicPublish():
            return pb.Pattern(basic_publish=pb.BasicPublishPattern())
        case m.BasicReturn():
            return pb.Pattern(
                basic_return=pb.BasicReturnPattern(
                    reply_code=concrete_or(pattern.reply_code),
                    exchange=concrete_or(pattern.exchange),
                    routing_key=concrete_or(pattern.routing_key),
                ),
            )
        case m.BasicAck():
            return pb.Pattern(
                basic_ack=pb.BasicAckPattern(
                    delivery_tag=concrete_or(pattern.delivery_tag),
                ),
            )
        case m.BasicNack():
            return pb.Pattern(
                basic_nack=pb.BasicNackPattern(
                    delivery_tag=concrete_or(pattern.delivery_tag),
                ),
            )
        case m.BasicReject():
            return pb.Pattern(
                basic_reject=pb.BasicRejectPattern(
                    delivery_tag=concrete_or(pattern.delivery_tag),
                ),
            )
        case m.BasicDeliver():
            return pb.Pattern(
                basic_deliver=pb.BasicDeliverPattern(
                    consumer_tag=concrete_or(pattern.consumer_tag),
                    delivery_tag=concrete_or(pattern.delivery_tag),
                ),
            )
        case m.BasicGet():
            return pb.Pattern(basic_get=pb.BasicGetPattern())
        case m.BasicGetOk():
            return pb.Pattern(
                basic_get_ok=pb.BasicGetOkPattern(
                    delivery_tag=concrete_or(pattern.delivery_tag),
                ),
            )
        case m.BasicGetEmpty():
            return pb.Pattern(basic_get_empty=pb.BasicGetEmptyPattern())


def pattern_from_proto(message: pb.Pattern) -> m.Method:
    match message.WhichOneof("method"):
        case None:
            raise ImposterError("no pattern given")
        case "connection_start":
            return m.ConnectionStart(
                version_major=cond_exact_field(
                    message.connection_start.HasField("version_major"),
                    message.connection_start.version_major,
                ),
                version_minor=cond_exact_field(
                    message.connection_start.HasField("version_minor"),
                    message.connection_start.version_minor,
                ),
                mechanisms=cond_exact_field(
                    message.connection_start.HasField("mechanisms"),
                    tuple(message.connection_start.mechanisms.items),
                ),
                locales=cond_exact_field(
                    message.connection_start.HasField("locales"),
                    tuple(message.connection_start.locales.items),
                ),
                publisher_confirms=cond_exact_field(
                    message.connection_start.HasField("publisher_confirms"),
                    message.connection_start.publisher_confirms,
                ),
                basic_nack=cond_exact_field(
                    message.connection_start.HasField("basic_nack"),
                    message.connection_start.basic_nack,
                ),
                consumer_cancel_notify=cond_exact_field(
                    message.connection_start.HasField("consumer_cancel_notify"),
                    message.connection_start.consumer_cancel_notify,
                ),
            )
        case "connection_start_ok":
            return m.ConnectionStartOk(
                mechanism=cond_exact_field(
                    message.connection_start_ok.HasField("mechanism"),
                    message.connection_start_ok.mechanism,
                ),
                locale=cond_exact_field(
                    message.connection_start_ok.HasField("locale"),
                    message.connection_start_ok.locale,
                ),
            )
        case "connection_tune":
            return m.ConnectionTune(
                channel_max=cond_exact_field(
                    message.connection_tune.HasField("channel_max"),
                    message.connection_tune.channel_max,
                ),
                frame_max=cond_exact_field(
                    message.connection_tune.HasField("frame_max"),
                    message.connection_tune.frame_max,
                ),
                heartbeat=cond_exact_field(
                    message.connection_tune.HasField("heartbeat"),
                    message.connection_tune.heartbeat,
                ),
            )
        case "connection_tune_ok":
            return m.ConnectionTuneOk(
                channel_max=cond_exact_field(
                    message.connection_tune_ok.HasField("channel_max"),
                    message.connection_tune_ok.channel_max,
                ),
                frame_max=cond_exact_field(
                    message.connection_tune_ok.HasField("frame_max"),
                    message.connection_tune_ok.frame_max,
                ),
                heartbeat=cond_exact_field(
                    message.connection_tune_ok.HasField("heartbeat"),
                    message.connection_tune_ok.heartbeat,
                ),
            )
        case "connection_open":
            return m.ConnectionOpen()
        case "connection_open_ok":
            return m.ConnectionOpenOk()
        case "connection_close":
            return m.ConnectionClose(
                reply_code=cond_exact_field(
                    message.connection_close.HasField("reply_code"),
                    message.connection_close.reply_code,
                ),
                reply_text=cond_exact_field(
                    message.connection_close.HasField("reply_text"),
                    message.connection_close.reply_text,
                ),
                class_id=cond_exact_field(
                    message.connection_close.HasField("class_id"),
                    message.connection_close.class_id,
                ),
                method_id=cond_exact_field(
                    message.connection_close.HasField("method_id"),
                    message.connection_close.method_id,
                ),
            )
        case "connection_close_ok":
            return m.ConnectionCloseOk()
        case "channel_open":
            return m.ChannelOpen()
        case "channel_open_ok":
            return m.ChannelOpenOk()
        case "channel_close":
            return m.ChannelClose(
                reply_code=cond_exact_field(
                    message.channel_close.HasField("reply_code"),
                    message.channel_close.reply_code,
                ),
                reply_text=cond_exact_field(
                    message.channel_close.HasField("reply_text"),
                    message.channel_close.reply_text,
                ),
                class_id=cond_exact_field(
                    message.channel_close.HasField("class_id"),
                    message.channel_close.class_id,
                ),
                method_id=cond_exact_field(
                    message.channel_close.HasField("method_id"),
                    message.channel_close.method_id,
                ),
            )
        case "channel_close_ok":
            return m.ChannelCloseOk()
        case "confirm_select":
            return m.ConfirmSelect()
        case "confirm_select_ok":
            return m.ConfirmSelectOk()
        case "exchange_declare":
            return m.ExchangeDeclare(
                exchange=cond_exact_field(
                    message.exchange_declare.HasField("exchange"),
                    message.exchange_declare.exchange,
                ),
            )
        case "exchange_declare_ok":
            return m.ExchangeDeclareOk()
        case "exchange_bind":
            return m.ExchangeBind()
        case "exchange_bind_ok":
            return m.ExchangeBindOk()
        case "queue_declare":
            return m.QueueDeclare()
        case "queue_declare_ok":
            return m.QueueDeclareOk()
        case "queue_bind":
            return m.QueueBind()
        case "queue_bind_ok":
            return m.QueueBindOk()
        case "basic_qos":
            return m.BasicQos()
        case "basic_qos_ok":
            return m.BasicQosOk()
        case "basic_consume":
            return m.BasicConsume()
        case "basic_consume_ok":
            return m.BasicConsumeOk()
        case "basic_cancel":
            return m.BasicCancel(
                consumer_tag=cond_exact_field(
                    message.basic_cancel.HasField("consumer_tag"),
                    message.basic_cancel.consumer_tag,
                ),
            )
        case "basic_cancel_ok":
            return m.BasicCancelOk(
                consumer_tag=cond_exact_field(
                    message.basic_cancel_ok.HasField("consumer_tag"),
                    message.basic_cancel_ok.consumer_tag,
                ),
            )
        case "basic_publish":
            return m.BasicPublish()
        case "basic_return":
            return m.BasicReturn(
                reply_code=cond_exact_field(
                    message.basic_return.HasField("reply_code"),
                    message.basic_return.reply_code,
                ),
                exchange=cond_exact_field(
                    message.basic_return.HasField("exchange"),
                    message.basic_return.exchange,
                ),
                routing_key=cond_exact_field(
                    message.basic_return.HasField("routing_key"),
                    message.basic_return.routing_key,
                ),
            )
        case "basic_ack":
            return m.BasicAck(
                delivery_tag=cond_exact_field(
                    message.basic_ack.HasField("delivery_tag"),
                    message.basic_ack.delivery_tag,
                ),
            )
        case "basic_nack":
            return m.BasicNack(
                delivery_tag=cond_exact_field(
                    message.basic_nack.HasField("delivery_tag"),
                    message.basic_nack.delivery_tag,
                ),
            )
        case "basic_reject":
            return m.BasicReject(
                delivery_tag=cond_exact_field(
                    message.basic_reject.HasField("delivery_tag"),
                    message.basic_reject.delivery_tag,
                ),
            )
        case "basic_deliver":
            return m.BasicDeliver(
                consumer_tag=cond_exact_field(
                    message.basic_deliver.HasField("consumer_tag"),
                    message.basic_deliver.consumer_tag,
                ),
                delivery_tag=cond_exact_field(
                    message.basic_deliver.HasField("delivery_tag"),
                    message.basic_deliver.delivery_tag,
                ),
            )
        case "basic_get":
            return m.BasicGet()
        case "basic_get_ok":
            return m.BasicGetOk(
                delivery_tag=cond_exact_field(
                    message.basic_get_ok.HasField("delivery_tag"),
                    message.basic_get_ok.delivery_tag,
                ),
            )
        case "basic_get_empty":
            return m.BasicGetEmpty()
