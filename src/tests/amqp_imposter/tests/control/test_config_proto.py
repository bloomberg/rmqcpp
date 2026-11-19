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
from google.protobuf.timestamp_pb2 import Timestamp

from amqp_imposter.amqp import session as se
from amqp_imposter.amqp import transcript as tr
from amqp_imposter.control import config_proto as cp
from amqp_imposter.control.v1 import imposter_pb2 as pb
from amqp_imposter.imposter import ImposterError
from amqp_imposter.model import constants as c
from amqp_imposter.model import domains as d
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import method_scripts as scr
from amqp_imposter.model import method_weights as mw
from amqp_imposter.model import methods as m
from amqp_imposter.model import negotiation_options as no

from ..helpers import concrete, filled

SAMPLES = 10

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
            channel_max=d.Short(7),
            frame_max=d.Long(8192),
            heartbeat=d.Short(60),
        ),
    ),
    replace(
        se.DEFAULT_SESSION_CONFIG,
        weights=(
            mw.WeightedSelector(m.BasicAck(delivery_tag=d.LongLong(2)), 0),
            mw.WeightedSelector(m.BasicNack(), 8),
        ),
    ),
    replace(
        se.DEFAULT_SESSION_CONFIG,
        idle_weights=(
            mw.WeightedSelector(mw.UNPROMPTED_CONNECTION_CLOSE, 0),
            mw.WeightedSelector(m.BasicDeliver(), 8),
        ),
    ),
    replace(
        se.DEFAULT_SESSION_CONFIG,
        script=(
            scr.ScriptRule(
                scr.Received(m.BasicPublish(mandatory=True)), then_send=m.BasicAck()
            ),
            scr.ScriptRule(
                scr.Sent(m.BasicAck(delivery_tag=d.LongLong(2))),
                then_send=m.BasicDeliver(
                    redelivered=True,
                    properties=mf.MessageProperties(message_id=d.ShortStr("m-1")),
                ),
            ),
        ),
        max_burst_length=3,
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
        no.DEFAULT_NEGOTIATION_OPTIONS, heartbeat=d.Short(60)
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
        "max_burst_length",
    }


def test_encoded_negotiation_is_total() -> None:
    message = cp.negotiation_to_proto(no.DEFAULT_NEGOTIATION_OPTIONS)

    assert {field.name for field, _ in message.ListFields()} == {
        field.name for field in pb.NegotiationOptions.DESCRIPTOR.fields
    }


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_selector_message_covers_exact_fields(kind: type[m.Method]) -> None:
    oneof_fields = pb.Selector.DESCRIPTOR.oneofs_by_name["method"].fields
    [message] = [
        one.message_type
        for one in oneof_fields
        if one.message_type is not None
        and one.message_type.name == kind.__name__ + "Selector"
    ]

    assert {one.name for one in message.fields} == set(m.exact_fields(kind))


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_empty_selector_compose_from_to(kind: type[m.Method]) -> None:
    assert cp.selector_from_proto(cp.selector_to_proto(kind())) == kind()


@pytest.mark.parametrize(
    "selector",
    (
        m.BasicAck(delivery_tag=d.LongLong(2)),
        m.BasicNack(delivery_tag=d.LongLong(0)),
        m.BasicCancel(consumer_tag=d.ShortStr("ctag")),
        m.BasicDeliver(consumer_tag=d.ShortStr("ctag"), delivery_tag=d.LongLong(1)),
        m.ConnectionClose(
            reply_code=d.Short(320), class_id=d.Short(0), method_id=d.Short(0)
        ),
        m.BasicReturn(
            reply_code=d.Short(312),
            exchange=d.ShortStr("ex"),
            routing_key=d.ShortStr("rk"),
        ),
        m.ConnectionStartOk(mechanism=d.ShortStr("PLAIN"), locale=d.ShortStr("en_US")),
        m.ConnectionStart(mechanisms=("PLAIN", "AMQPLAIN"), locales=("en_US", "en_GB")),
    ),
    ids=lambda p: type(p).__name__,
)
def test_selector_compose_from_to(selector: m.Method) -> None:
    assert cp.selector_from_proto(cp.selector_to_proto(selector)) == selector


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_filled_selector_compose_from_to(kind: type[m.Method]) -> None:
    selector = filled(kind)

    assert cp.selector_from_proto(cp.selector_to_proto(selector)) == selector


def test_selector_keeps_holes() -> None:
    selector = cp.selector_from_proto(cp.selector_to_proto(m.BasicAck()))

    assert m.is_method_pattern(selector)


def test_selector_fills_holes() -> None:
    selector = cp.selector_from_proto(
        cp.selector_to_proto(m.BasicAck(delivery_tag=d.LongLong(2)))
    )

    assert not m.is_method_pattern(selector)


def test_weighted_compose_from_to() -> None:
    weighted = mw.WeightedSelector(m.BasicNack(delivery_tag=d.LongLong(2)), 8)

    assert cp.weighted_from_proto(cp.weighted_to_proto(weighted)) == weighted


def test_schema_base_weight_matches_the_model() -> None:
    assert pb.BASE_WEIGHT == mw.BASE_WEIGHT


def test_missing_weight_is_base() -> None:
    message = pb.Weighted(selector=pb.Selector(basic_ack=pb.BasicAckSelector()))

    assert cp.weighted_from_proto(message).weight == mw.BASE_WEIGHT


def test_zero_weight_is_kept() -> None:
    message = pb.Weighted(
        selector=pb.Selector(basic_ack=pb.BasicAckSelector()), weight=0
    )

    assert cp.weighted_from_proto(message).weight == 0


def test_empty_selector_is_bad_value() -> None:
    with pytest.raises(ImposterError):
        cp.selector_from_proto(pb.Selector())


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_pattern_message_covers_all_fields(kind: type[m.Method]) -> None:
    message = pb.DESCRIPTOR.message_types_by_name[kind.__name__ + "Pattern"]

    assert {one.name for one in message.fields} == set(m.get_fields(kind()))


@pytest.mark.parametrize("seed", range(SAMPLES))
@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_pattern_compose_from_to(kind: type[m.Method], seed: int) -> None:
    for pattern in (kind(), filled(kind), concrete(kind, seed)):
        assert cp.pattern_from_proto(cp.pattern_to_proto(pattern)) == pattern


def test_empty_pattern_is_bad_value() -> None:
    with pytest.raises(ImposterError):
        cp.pattern_from_proto(pb.Pattern())


@pytest.mark.parametrize(
    "rule",
    (
        scr.ScriptRule(
            scr.Received(m.BasicPublish(mandatory=True)),
            then_send=m.BasicNack(requeue=True),
        ),
        scr.ScriptRule(
            scr.Sent(m.BasicCancel(consumer_tag=d.ShortStr("ctag"), no_wait=True)),
            then_send=m.ConnectionClose(),
        ),
        scr.ScriptRule(scr.Sent(m.BasicPublish()), then_send=m.BasicPublish()),
    ),
    ids=lambda rule: type(rule.when).__name__,
)
def test_script_compose_from_to(rule: scr.ScriptRule) -> None:
    assert cp.script_rule_from_proto(cp.script_rule_to_proto(rule)) == rule


# fmt: off
BAD_CONFIGS: tuple[pb.SessionConfig, ...] = (
    pb.SessionConfig(stop_weight=0),
    pb.SessionConfig(max_burst_length=0),
    pb.SessionConfig(weights=(pb.Weighted(weight=1),)),
    pb.SessionConfig(weights=(pb.Weighted(selector=pb.Selector(), weight=1),)),
    pb.SessionConfig(idle_weights=(pb.Weighted(weight=1),)),
    pb.SessionConfig(idle_weights=(pb.Weighted(selector=pb.Selector(), weight=1),)),
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
    pb.SessionConfig(negotiation_options=pb.NegotiationOptions(heartbeat=d.Short.MAX + 1)),
    pb.SessionConfig(negotiation_options=pb.NegotiationOptions(channel_max=d.Short.MAX + 1)),
    pb.SessionConfig(weights=(pb.Weighted(selector=pb.Selector(basic_cancel=pb.BasicCancelSelector(consumer_tag="t" * (d.ShortStr.MAX_OCTETS + 1)))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(connection_close=pb.ConnectionClosePattern(reply_code=d.Short.MAX + 1))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(basic_deliver=pb.BasicDeliverPattern(routing_key="k" * (d.ShortStr.MAX_OCTETS + 1)))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(basic_deliver=pb.BasicDeliverPattern(properties=pb.MessageProperties(priority=d.Octet.MAX + 1)))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(basic_deliver=pb.BasicDeliverPattern(properties=pb.MessageProperties(headers=pb.AmqpTable(entries={"h" * (d.ShortStr.MAX_OCTETS + 1): pb.AmqpValue(long=1)}))))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(basic_deliver=pb.BasicDeliverPattern(properties=pb.MessageProperties(headers=pb.AmqpTable(entries={"h": pb.AmqpValue(decimal="abc")}))))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(basic_deliver=pb.BasicDeliverPattern(properties=pb.MessageProperties(headers=pb.AmqpTable(entries={"h": pb.AmqpValue(decimal="1E+10")}))))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(basic_deliver=pb.BasicDeliverPattern(properties=pb.MessageProperties(headers=pb.AmqpTable(entries={"h": pb.AmqpValue(timestamp=Timestamp(seconds=-1))}))))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(basic_deliver=pb.BasicDeliverPattern(properties=pb.MessageProperties(headers=pb.AmqpTable(entries={"h": pb.AmqpValue(timestamp=Timestamp(seconds=1, nanos=1))}))))),)),
    pb.SessionConfig(script=(pb.Scripted(sent=pb.Pattern(basic_ack=pb.BasicAckPattern()), then_send=pb.Pattern(basic_deliver=pb.BasicDeliverPattern(properties=pb.MessageProperties(headers=pb.AmqpTable(entries={"h": pb.AmqpValue(timestamp=Timestamp(seconds=d.Timestamp.MAX + 1))}))))),)),
    pb.SessionConfig(script=(pb.Scripted(received=pb.Pattern(queue_declare=pb.QueueDeclarePattern(queue="")), then_send=pb.Pattern(queue_declare_ok=pb.QueueDeclareOkPattern(queue=""))),)),
    pb.SessionConfig(script=(pb.Scripted(received=pb.Pattern(basic_consume=pb.BasicConsumePattern(consumer_tag="")), then_send=pb.Pattern(basic_consume_ok=pb.BasicConsumeOkPattern(consumer_tag=""))),)),
    pb.SessionConfig(script=(pb.Scripted(received=pb.Pattern(basic_publish=pb.BasicPublishPattern(mandatory=True)), then_send=pb.Pattern(basic_return=pb.BasicReturnPattern(reply_text=""))),)),
    pb.SessionConfig(script=(pb.Scripted(received=pb.Pattern(queue_declare=pb.QueueDeclarePattern(arguments=pb.AmqpTable(entries={"x-max-length": pb.AmqpValue(decimal="NaN")}))), then_send=pb.Pattern(basic_ack=pb.BasicAckPattern())),)),
)
# fmt: on
@pytest.mark.parametrize("message", BAD_CONFIGS, ids=range(len(BAD_CONFIGS)))
def test_errors(message: pb.SessionConfig) -> None:
    with pytest.raises(ImposterError) as e:
        cp.from_proto(message)

    assert str(e.value)
