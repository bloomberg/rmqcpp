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

from dataclasses import replace

import pytest

from amqp_imposter.amqp import session as se
from amqp_imposter.amqp import transcript as tr
from amqp_imposter.control import config_proto as cp
from amqp_imposter.control.v1 import imposter_pb2 as pb
from amqp_imposter.imposter import ImposterError
from amqp_imposter.model import constants as c
from amqp_imposter.model import method_scripts as scr
from amqp_imposter.model import method_weights as mw
from amqp_imposter.model import methods as m
from amqp_imposter.model import negotiation_options as no

from ..helpers import filled

CONFIGS = (
    se.DEFAULT_SESSION_CONFIG,
    replace(se.DEFAULT_SESSION_CONFIG, seed=7, stop_weight=1, idle_millis=1),
    replace(se.DEFAULT_SESSION_CONFIG, max_buffer=4096, max_transcript_lines=1),
    replace(
        se.DEFAULT_SESSION_CONFIG,
        negotiation_options=no.NegotiationOptions(
            mechanisms=("PLAIN", "AMQPLAIN"),
            locales=("en_US", "en_GB"),
            publisher_confirms=False,
            basic_nack=False,
            consumer_cancel_notify=False,
            channel_max=7,
            frame_max=8192,
            heartbeat=60,
        ),
    ),
    replace(
        se.DEFAULT_SESSION_CONFIG,
        weights=(
            mw.WeightedPattern(m.BasicAck(delivery_tag=2), 0),
            mw.WeightedPattern(m.BasicNack(), 8),
        ),
    ),
    replace(
        se.DEFAULT_SESSION_CONFIG,
        idle_weights=(
            mw.WeightedPattern(mw.UNPROMPTED_CONNECTION_CLOSE, 0),
            mw.WeightedPattern(m.BasicDeliver(), 8),
        ),
    ),
    replace(
        se.DEFAULT_SESSION_CONFIG,
        script=(
            scr.ScriptRule(scr.Received(m.BasicPublish()), then_send=m.BasicAck()),
            scr.ScriptRule(
                scr.Sent(m.BasicAck(delivery_tag=2)), then_send=m.BasicDeliver()
            ),
        ),
        max_sends=3,
    ),
)


@pytest.mark.parametrize("config", CONFIGS, ids=range(len(CONFIGS)))
def test_compose_from_to(config: se.SessionConfig) -> None:
    assert cp.from_proto(cp.to_proto(config)) == config


@pytest.mark.parametrize("config", CONFIGS, ids=range(len(CONFIGS)))
def test_survives_the_wire(config: se.SessionConfig) -> None:
    encoded = cp.to_proto(config).SerializeToString()

    assert cp.from_proto(pb.SessionConfig.FromString(encoded)) == config


def test_all_fields_missing() -> None:
    assert cp.from_proto(pb.SessionConfig()) == se.DEFAULT_SESSION_CONFIG


def test_some_fields_missing() -> None:
    assert cp.from_proto(pb.SessionConfig(seed=7)) == replace(
        se.DEFAULT_SESSION_CONFIG, seed=7
    )


def test_negotiation_partial() -> None:
    message = pb.SessionConfig(negotiation_options=pb.NegotiationOptions(heartbeat=60))

    assert cp.from_proto(message).negotiation_options == replace(
        no.DEFAULT_NEGOTIATION_OPTIONS, heartbeat=60
    )


def test_encoded_config_is_total() -> None:
    message = cp.to_proto(se.DEFAULT_SESSION_CONFIG)

    assert {field.name for field, _ in message.ListFields()} == {
        "seed",
        "negotiation_options",
        "stop_weight",
        "idle_millis",
        "max_buffer",
        "max_transcript_lines",
        "max_sends",
    }


def test_encoded_negotiation_is_total() -> None:
    message = cp.negotiation_to_proto(no.DEFAULT_NEGOTIATION_OPTIONS)

    assert {field.name for field, _ in message.ListFields()} == {
        field.name for field in pb.NegotiationOptions.DESCRIPTOR.fields
    }


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_pattern_message_covers_exact_fields(kind: type[m.Method]) -> None:
    arms = pb.Pattern.DESCRIPTOR.oneofs_by_name["method"].fields
    [message] = [
        one.message_type
        for one in arms
        if one.message_type is not None
        and one.message_type.name == kind.__name__ + "Pattern"
    ]

    assert {one.name for one in message.fields} == set(m.exact_fields(kind))


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_empty_pattern_compose_from_to(kind: type[m.Method]) -> None:
    assert cp.pattern_from_proto(cp.pattern_to_proto(kind())) == kind()


@pytest.mark.parametrize(
    "pattern",
    (
        m.BasicAck(delivery_tag=2),
        m.BasicNack(delivery_tag=0),
        m.BasicCancel(consumer_tag="ctag"),
        m.BasicDeliver(consumer_tag="ctag", delivery_tag=1),
        m.ConnectionClose(reply_code=320, class_id=0, method_id=0),
        m.BasicReturn(reply_code=312, exchange="ex", routing_key="rk"),
        m.ConnectionStartOk(mechanism="PLAIN", locale="en_US"),
        m.ConnectionStart(mechanisms=("PLAIN", "AMQPLAIN"), locales=("en_US", "en_GB")),
    ),
    ids=lambda p: type(p).__name__,
)
def test_pattern_compose_from_to(pattern: m.Method) -> None:
    assert cp.pattern_from_proto(cp.pattern_to_proto(pattern)) == pattern


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_filled_pattern_compose_from_to(kind: type[m.Method]) -> None:
    pattern = filled(kind)

    assert cp.pattern_from_proto(cp.pattern_to_proto(pattern)) == pattern


def test_pattern_keeps_holes() -> None:
    pattern = cp.pattern_from_proto(cp.pattern_to_proto(m.BasicAck()))

    assert m.is_method_pattern(pattern)


def test_pattern_fills_holes() -> None:
    pattern = cp.pattern_from_proto(cp.pattern_to_proto(m.BasicAck(delivery_tag=2)))

    assert not m.is_method_pattern(pattern)


def test_weighted_compose_from_to() -> None:
    weighted = mw.WeightedPattern(m.BasicNack(delivery_tag=2), 8)

    assert cp.weighted_from_proto(cp.weighted_to_proto(weighted)) == weighted


def test_schema_base_weight_matches_the_model() -> None:
    assert pb.BASE_WEIGHT == mw.BASE_WEIGHT


def test_missing_weight_is_base() -> None:
    message = pb.Weighted(pattern=pb.Pattern(basic_ack=pb.BasicAckPattern()))

    assert cp.weighted_from_proto(message).weight == mw.BASE_WEIGHT


def test_zero_weight_is_kept() -> None:
    message = pb.Weighted(pattern=pb.Pattern(basic_ack=pb.BasicAckPattern()), weight=0)

    assert cp.weighted_from_proto(message).weight == 0


def test_empty_pattern_is_bad_value() -> None:
    with pytest.raises(ImposterError):
        cp.pattern_from_proto(pb.Pattern())


@pytest.mark.parametrize(
    "rule",
    (
        scr.ScriptRule(scr.Received(m.BasicPublish()), then_send=m.BasicNack()),
        scr.ScriptRule(
            scr.Sent(m.BasicCancel(consumer_tag="ctag")),
            then_send=m.ConnectionClose(),
        ),
    ),
    ids=lambda rule: type(rule.when).__name__,
)
def test_script_compose_from_to(rule: scr.ScriptRule) -> None:
    assert cp.script_rule_from_proto(cp.script_rule_to_proto(rule)) == rule


# fmt: off
BAD_CONFIGS: tuple[pb.SessionConfig, ...] = (
    pb.SessionConfig(stop_weight=0),
    pb.SessionConfig(max_sends=0),
    pb.SessionConfig(weights=(pb.Weighted(weight=1),)),
    pb.SessionConfig(weights=(pb.Weighted(pattern=pb.Pattern(), weight=1),)),
    pb.SessionConfig(idle_weights=(pb.Weighted(weight=1),)),
    pb.SessionConfig(idle_weights=(pb.Weighted(pattern=pb.Pattern(), weight=1),)),
    pb.SessionConfig(script=(pb.Scripted(then_send=pb.Pattern(basic_ack=pb.BasicAckPattern())),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern())),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern()),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(), then_send=pb.Pattern(basic_ack=pb.BasicAckPattern())),)),
    pb.SessionConfig(script=(pb.Scripted(received=pb.Pattern(), then_send=pb.Pattern(basic_ack=pb.BasicAckPattern())),)),
    pb.SessionConfig(idle_millis=0),
    pb.SessionConfig(max_buffer=0),
    pb.SessionConfig(max_transcript_lines=0),
    pb.SessionConfig(max_transcript_lines=tr.HARD_MAX_TRANSCRIPT_LINES + 1),
    pb.SessionConfig(negotiation_options=pb.NegotiationOptions(mechanisms=pb.StringList())),
    pb.SessionConfig(negotiation_options=pb.NegotiationOptions(locales=pb.StringList())),
    pb.SessionConfig(negotiation_options=pb.NegotiationOptions(frame_max=100)),
    pb.SessionConfig(negotiation_options=pb.NegotiationOptions(frame_max=c.FRAME_MIN_FLOOR - 1)),
    pb.SessionConfig(negotiation_options=pb.NegotiationOptions(heartbeat=c.HEARTBEAT_MAX + 1)),
    pb.SessionConfig(negotiation_options=pb.NegotiationOptions(channel_max=c.CHANNEL_MAX_CEIL + 1)),
)
# fmt: on
@pytest.mark.parametrize("message", BAD_CONFIGS, ids=range(len(BAD_CONFIGS)))
def test_errors(message: pb.SessionConfig) -> None:
    with pytest.raises(ImposterError) as e:
        cp.from_proto(message)

    assert str(e.value)
