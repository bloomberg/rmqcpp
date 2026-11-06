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

import pytest

from amqp_imposter.amqp import method_framing as fr
from amqp_imposter.amqp import session as se
from amqp_imposter.amqp.transcript import TranscriptEvent as ev
from amqp_imposter.model import methods as m
from amqp_imposter.model.method_scripts import Received, ScriptRule
from amqp_imposter.model.method_weights import WeightedPattern

from .helpers import (
    B_BAD_FRAME,
    B_OVERLONG_FRAME,
    BASIC_CONSUME,
    BASIC_PUBLISH,
    CHANNEL,
    CHANNEL_CLOSE,
    CONNECTION_START,
    CONNECTION_START_OK,
    CONNECTION_TUNE_OK,
    GLOBAL_CHANNEL,
    OTHER_CHANNEL,
    SEED,
    STOP_WEIGHT_DRAIN,
    STOP_WEIGHT_LIKELY,
    encoded_method,
    filter_action_method_kinds,
    filter_action_methods,
    handshake,
    has_drop_action,
    session_connected,
    transcript_events,
    transcript_method_lines,
    transcript_time_normalised,
)

IDLES = 20
MANY_IDLES = 200


def test_session_starts() -> None:
    session = se.Session(se.SessionConfig(seed=SEED, stop_weight=STOP_WEIGHT_DRAIN))

    assert transcript_events(session.transcript()) == (ev.SESSION_STARTED,)
    assert session.transcript()[0].index == 0


def test_connect_then_disconnect() -> None:
    session = se.Session(se.SessionConfig(seed=SEED, stop_weight=STOP_WEIGHT_DRAIN))

    assert session.on_connected() == ()
    assert session.on_disconnected() == ()
    assert transcript_events(session.transcript()) == (
        ev.SESSION_STARTED,
        ev.TCP_CONNECTED,
        ev.TCP_DOWN,
    )


def test_rejected() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)

    acts = session.on_rejected()

    assert has_drop_action(acts)
    assert transcript_events(session.transcript())[-1] == ev.TCP_REJECTED


def test_ingress_protocol_header() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)

    acts = session.on_ingress(fr.PROTOCOL_HEADER_091)

    assert filter_action_methods(acts) == (
        fr.MethodOnChannel(GLOBAL_CHANNEL, CONNECTION_START),
    )
    assert transcript_events(session.transcript())[-2:] == (
        ev.PROTOCOL_HEADER,
        ev.METHOD_EGRESS,
    )


def test_handshake() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)

    acts = handshake(session)

    assert filter_action_method_kinds(acts) == [
        m.ConnectionStart,
        m.ConnectionTune,
        m.ConnectionOpenOk,
    ]
    assert ev.METHOD_INGRESS in transcript_events(session.transcript())


def test_method_before_protocol_header() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)

    acts = session.on_ingress(encoded_method(CONNECTION_START_OK))

    assert has_drop_action(acts)
    assert transcript_events(session.transcript())[-1] == ev.METHOD_UNREPRESENTABLE


def test_second_protocol_header() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    session.on_ingress(fr.PROTOCOL_HEADER_091)

    acts = session.on_ingress(fr.PROTOCOL_HEADER_091)

    assert has_drop_action(acts)
    assert transcript_events(session.transcript())[-1] == ev.METHOD_UNREPRESENTABLE


def test_unsupported_protocol_header() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    amqp_0_10 = b"AMQP\x01\x01\x00\x0a"

    acts = session.on_ingress(amqp_0_10)

    assert acts == (
        se.Write(fr.PROTOCOL_HEADER_091),
        se.Drop(se.REASON_UNSUPPORTED_PROTOCOL),
    )
    assert session.transcript()[-1].event == ev.METHOD_UNREPRESENTABLE
    assert session.transcript()[-1].wire_bytes == amqp_0_10


def test_malformed_frame() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    session.on_ingress(fr.PROTOCOL_HEADER_091)

    acts = session.on_ingress(B_BAD_FRAME)

    assert has_drop_action(acts)
    assert transcript_events(session.transcript())[-1] == ev.FRAME_MALFORMED


@pytest.mark.parametrize(
    ("method", "channel"),
    (
        (CONNECTION_START_OK, CHANNEL),
        (m.ChannelOpen(), GLOBAL_CHANNEL),
    ),
    ids=("connection_not_on_global", "channel_on_global"),
)
def test_bad_channels(method: m.Method, channel: int) -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    session.on_ingress(fr.PROTOCOL_HEADER_091)

    assert has_drop_action(session.on_ingress(encoded_method(method, channel)))


def test_two_channels_at_once() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)
    session.on_ingress(encoded_method(m.ChannelOpen(), CHANNEL))

    acts = session.on_ingress(encoded_method(m.ChannelOpen(), OTHER_CHANNEL))

    assert has_drop_action(acts)


def test_wrong_channel() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)
    session.on_ingress(encoded_method(m.ChannelOpen(), CHANNEL))

    acts = session.on_ingress(encoded_method(BASIC_PUBLISH, OTHER_CHANNEL))

    assert has_drop_action(acts)


def test_duplicate_open() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)
    session.on_ingress(encoded_method(m.ChannelOpen(), CHANNEL))

    acts = session.on_ingress(encoded_method(m.ChannelOpen(), CHANNEL))

    assert not has_drop_action(acts)
    assert filter_action_method_kinds(acts) == [m.ConnectionClose]


def test_channel_close() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)
    session.on_ingress(encoded_method(m.ChannelOpen(), CHANNEL))
    session.on_ingress(encoded_method(CHANNEL_CLOSE, CHANNEL))

    acts = session.on_ingress(encoded_method(m.ChannelOpen(), OTHER_CHANNEL))

    assert not has_drop_action(acts)
    assert filter_action_methods(acts) == (
        fr.MethodOnChannel(OTHER_CHANNEL, m.ChannelOpenOk()),
    )


def test_reconnect_clean_slate() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    session.on_ingress(fr.PROTOCOL_HEADER_091)
    session.on_ingress(encoded_method(CONNECTION_START_OK))
    session.on_disconnected()

    session.on_connected()
    acts = session.on_ingress(fr.PROTOCOL_HEADER_091)

    assert not has_drop_action(acts)
    assert filter_action_method_kinds(acts) == [m.ConnectionStart]


def test_nothing_to_send() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)

    i = len(session.transcript())
    acts: tuple[se.SocketAction, ...] = ()

    acts += session.on_idle()
    acts += session.on_idle()
    acts += session.on_idle()
    acts += session.on_idle()

    assert filter_action_methods(acts) == ()
    assert session.transcript()[i:] == ()  # no-op timeouts only


def test_zero_weight_never_sent() -> None:
    session = se.Session(
        se.SessionConfig(seed=SEED, weights=(WeightedPattern(m.ConnectionStart(), 0),))
    )
    session.on_connected()

    acts = session.on_ingress(fr.PROTOCOL_HEADER_091)
    acts += session.on_idle()
    acts += session.on_idle()
    acts += session.on_idle()

    assert filter_action_methods(acts) == ()


def test_idle_weights_iff_idle() -> None:
    session = se.Session(
        se.SessionConfig(
            seed=SEED,
            stop_weight=STOP_WEIGHT_DRAIN,
            weights=(WeightedPattern(m.ConnectionStart(), 0),),
            idle_weights=(WeightedPattern(m.ConnectionStart()),),
        )
    )
    session.on_connected()

    assert filter_action_methods(session.on_ingress(fr.PROTOCOL_HEADER_091)) == ()

    idle_acts = tuple(act for _ in range(IDLES) for act in session.on_idle())

    assert filter_action_method_kinds(idle_acts) == [m.ConnectionStart]


def test_script_method_on_receive() -> None:
    session = se.Session(
        se.SessionConfig(
            seed=SEED,
            stop_weight=STOP_WEIGHT_DRAIN,
            weights=(WeightedPattern(m.ConnectionTune(), 0),),
            script=(
                ScriptRule(
                    Received(m.ConnectionStartOk()), then_send=m.ConnectionTune()
                ),
            ),
        )
    )
    session.on_connected()
    session.on_ingress(fr.PROTOCOL_HEADER_091)

    acts = session.on_ingress(encoded_method(CONNECTION_START_OK))

    assert filter_action_method_kinds(acts) == [m.ConnectionTune]


def test_deterministic() -> None:
    first, second = (
        session_connected(STOP_WEIGHT_DRAIN),
        session_connected(STOP_WEIGHT_DRAIN),
    )

    handshake(first)
    handshake(second)

    assert transcript_time_normalised(first.transcript()) == transcript_time_normalised(
        second.transcript()
    )


def test_reset_seed() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)
    before = transcript_time_normalised(session.transcript())

    session.reset()
    session.on_connected()
    handshake(session)

    assert transcript_time_normalised(session.transcript()) == before


def test_reset_transcript() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)

    session.reset()

    assert transcript_events(session.transcript()) == (ev.SESSION_STARTED,)


def test_reset_config() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    replacement = se.SessionConfig(seed=99, idle_millis=7)

    session.reset(replacement)

    assert session.config == replacement


def test_transcript_time_order() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)
    lines = session.transcript()

    assert [line.index for line in lines] == list(range(len(lines)))
    assert [line.micros for line in lines] == sorted(line.micros for line in lines)


def test_greeting() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    assert session.is_greeted() is False

    session.on_ingress(fr.PROTOCOL_HEADER_091)
    assert session.is_greeted() is True

    session.on_disconnected()
    assert session.is_greeted() is False


def test_reset_greeting() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    session.on_ingress(fr.PROTOCOL_HEADER_091)

    session.reset()

    assert session.is_greeted() is False


def test_reset_not_total_ingress_count() -> None:
    session = session_connected(STOP_WEIGHT_DRAIN)
    handshake(session)
    before = session.total_ingress_methods
    assert before > 0

    session.reset()

    assert session.total_ingress_methods == before


def test_ingress_buffer_maxed() -> None:
    session = se.Session(
        se.SessionConfig(seed=SEED, stop_weight=STOP_WEIGHT_DRAIN, max_buffer=100)
    )
    session.on_connected()
    session.on_ingress(fr.PROTOCOL_HEADER_091)

    acts = session.on_ingress(B_OVERLONG_FRAME)

    assert has_drop_action(acts)
    assert transcript_events(session.transcript())[-1] == ev.FRAME_MALFORMED


def test_transcript_capped() -> None:
    session = se.Session(
        se.SessionConfig(
            seed=SEED, stop_weight=STOP_WEIGHT_DRAIN, max_transcript_lines=2
        )
    )
    session.on_connected()
    session.on_disconnected()

    lines = session.transcript()

    assert len(lines) == 2
    assert lines[0].index == 1


def test_transcribe_consumer() -> None:
    session = session_connected(STOP_WEIGHT_LIKELY)

    for setup_part in (
        fr.PROTOCOL_HEADER_091,
        encoded_method(CONNECTION_START_OK),
        encoded_method(CONNECTION_TUNE_OK),
        encoded_method(m.ConnectionOpen()),
        encoded_method(m.ChannelOpen(), CHANNEL),
        encoded_method(BASIC_CONSUME, CHANNEL),
    ):
        i = len(session.transcript())
        session.on_ingress(setup_part)
        for _ in range(IDLES):
            session.on_idle()
        assert all(
            line.event in (ev.METHOD_INGRESS, ev.METHOD_EGRESS)
            for line in transcript_method_lines(session.transcript()[i:])
        )

    i = len(session.transcript())
    for _ in range(MANY_IDLES):
        session.on_idle()

    lines = session.transcript()[i:]  # cut the transcript to the idle part
    assert lines[0].event == ev.IDLE_TIMEOUT

    method_lines = transcript_method_lines(lines)
    assert method_lines
    assert all(line.event == ev.METHOD_EGRESS for line in method_lines)
