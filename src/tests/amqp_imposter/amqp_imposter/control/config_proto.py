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
from ..model import methods as m
from ..model.domains import BadDomain, Long, Short
from ..model.method_scripts import Received, ScriptRule, Sent
from ..model.method_weights import BASE_WEIGHT, BadWeight, WeightedSelector
from ..model.negotiation_options import (
    DEFAULT_NEGOTIATION_OPTIONS,
    NegotiationOptions,
)
from .method_proto import method_oneof
from .v1 import imposter_pb2 as pb

SELECTORS = method_oneof(pb.Selector, "method", m.METHOD_KINDS, "Selector")
PATTERNS = method_oneof(pb.Pattern, "method", m.METHOD_KINDS, "Pattern")


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
        max_burst_length=config.max_burst_length,
        idle_millis=config.idle_millis,
        max_buffer=config.max_buffer,
        max_transcript_lines=config.max_transcript_lines,
    )


def weights_from_proto(
    messages: Sequence[pb.Weighted], default: tuple[WeightedSelector, ...]
) -> tuple[WeightedSelector, ...]:
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
            max_burst_length=message.max_burst_length
            if message.HasField("max_burst_length")
            else default.max_burst_length,
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
    if config.max_burst_length <= 0:
        raise ImposterError(
            f"max_burst_length must be positive: {config.max_burst_length}",
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

    try:
        options = NegotiationOptions(
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
            channel_max=Short(message.channel_max)
            if message.HasField("channel_max")
            else default.channel_max,
            frame_max=Long(message.frame_max)
            if message.HasField("frame_max")
            else default.frame_max,
            heartbeat=Short(message.heartbeat)
            if message.HasField("heartbeat")
            else default.heartbeat,
        )
    except BadDomain as bad:
        raise ImposterError(str(bad)) from bad

    return validated_negotiation_options(options)


def validated_negotiation_options(options: NegotiationOptions) -> NegotiationOptions:
    if options.frame_max < c.FRAME_MIN_FLOOR:
        raise ImposterError(
            f"frame_max below {c.FRAME_MIN_FLOOR}: {options.frame_max}",
        )
    return options


def weighted_to_proto(weighted: WeightedSelector) -> pb.Weighted:
    return pb.Weighted(
        selector=selector_to_proto(weighted.selector), weight=weighted.weight
    )


def weighted_from_proto(message: pb.Weighted) -> WeightedSelector:
    weight = message.weight if message.HasField("weight") else BASE_WEIGHT

    try:
        return WeightedSelector(selector_from_proto(message.selector), weight)
    except (BadWeight, m.BadSelector) as bad:
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
    return ScriptRule(when_from_proto(message), pattern_from_proto(message.then_send))


def selector_to_proto(selector: m.Method) -> pb.Selector:
    return SELECTORS.to_proto(selector)


def selector_from_proto(message: pb.Selector) -> m.Method:
    return SELECTORS.from_proto(message)


def pattern_to_proto(pattern: m.Method) -> pb.Pattern:
    return PATTERNS.to_proto(pattern)


def pattern_from_proto(message: pb.Pattern) -> m.Method:
    return PATTERNS.from_proto(message)
