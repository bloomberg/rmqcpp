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

import asyncio
from collections.abc import Callable
from dataclasses import replace

from amqp_imposter.amqp import method_framing as fr
from amqp_imposter.amqp import session as se
from amqp_imposter.amqp.transcript import (
    TranscriptEvent,
    TranscriptLine,
)
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m
from amqp_imposter.model.method_scripts import Received, ScriptRule
from amqp_imposter.model.method_weights import (
    BASE_WEIGHT,
    WeightedPattern,
)

FRAME_MAX = 131072
GLOBAL_CHANNEL = 0
CHANNEL = 1
OTHER_CHANNEL = 2
SEED = 0
STOP_WEIGHT_DRAIN = 1
STOP_WEIGHT_LIKELY = 2 * BASE_WEIGHT

CONNECTION_CLOSE = m.ConnectionClose(
    mf.ReplyCode.SUCCESS,
    "Normal shutdown",
    mf.UNPROMPTED_CLASS_ID,
    mf.UNPROMPTED_METHOD_ID,
)
CONNECTION_START = m.ConnectionStart(
    version_major=0,
    version_minor=9,
    mechanisms=("PLAIN",),
    locales=("en_US",),
    publisher_confirms=True,
    basic_nack=True,
    consumer_cancel_notify=True,
)
CONNECTION_START_OK = m.ConnectionStartOk(mechanism="PLAIN", locale="en_US")
CONNECTION_TUNE_OK = m.ConnectionTuneOk(channel_max=2047, frame_max=131072, heartbeat=0)
CHANNEL_CLOSE = m.ChannelClose(
    reply_code=mf.ReplyCode.SUCCESS,
    reply_text="Normal shutdown",
    class_id=mf.UNPROMPTED_CLASS_ID,
    method_id=mf.UNPROMPTED_METHOD_ID,
)
BASIC_PUBLISH = m.BasicPublish(exchange="ex", routing_key="rk", body=b"x")
BASIC_CONSUME = m.BasicConsume(
    queue="q",
    consumer_tag="ctag",
    no_local=False,
    no_ack=False,
    no_wait=False,
    exclusive=False,
    arguments=None,
)

B_BAD_FRAME = b"\x63\x00\x01\x00\x00\x00\x01\x00\xce"
B_OVERLONG_FRAME = b"\x01\x00\x01" + (10_000_000).to_bytes(4, "big") + b"\x00" * 200


def encoded_method(method: m.Method, channel: int = GLOBAL_CHANNEL) -> bytes:
    return fr.encode(fr.MethodOnChannel(channel, method), FRAME_MAX)


def filter_action_methods(
    acts: tuple[se.SocketAction, ...],
) -> tuple[fr.MethodOnChannel, ...]:
    sent: list[fr.MethodOnChannel] = []

    for act in acts:
        match act:
            case se.Write():
                items, _ = fr.decode(act.payload, FRAME_MAX)
                sent.extend(
                    item for item in items if isinstance(item, fr.MethodOnChannel)
                )
            case _:
                continue

    return tuple(sent)


def filter_action_method_kinds(
    acts: tuple[se.SocketAction, ...],
) -> list[type[m.Method]]:
    return [type(act.method) for act in filter_action_methods(acts)]


def has_drop_action(acts: tuple[se.SocketAction, ...]) -> bool:
    return any(isinstance(act, se.Drop) for act in acts)


def transcript_events(lines: tuple[TranscriptLine, ...]) -> tuple[TranscriptEvent, ...]:
    return tuple(line.event for line in lines)


def transcript_time_normalised(
    lines: tuple[TranscriptLine, ...],
) -> tuple[TranscriptLine, ...]:
    return tuple(replace(line, micros=0, epoch_micros=0) for line in lines)


def transcript_method_lines(
    lines: tuple[TranscriptLine, ...],
) -> tuple[TranscriptLine, ...]:
    return tuple(line for line in lines if line.method is not None)


def handshake(session: se.Session) -> tuple[se.SocketAction, ...]:
    acts = list(session.on_ingress(fr.PROTOCOL_HEADER_091))
    acts.extend(session.on_ingress(encoded_method(CONNECTION_START_OK)))
    acts.extend(session.on_ingress(encoded_method(CONNECTION_TUNE_OK)))
    acts.extend(session.on_ingress(encoded_method(m.ConnectionOpen())))
    return tuple(acts)


# avoid drawing the stop pattern here
GREETING_WEIGHTS = (WeightedPattern(m.ConnectionStart(), 10**9),)
SCRIPTED_REPLIES = (
    ScriptRule(Received(m.ConnectionStartOk()), then_send=m.ConnectionTune()),
    ScriptRule(Received(m.ConnectionOpen()), then_send=m.ConnectionOpenOk()),
    ScriptRule(Received(m.ConnectionClose()), then_send=m.ConnectionCloseOk()),
    ScriptRule(Received(m.ChannelOpen()), then_send=m.ChannelOpenOk()),
    ScriptRule(
        Received(m.ChannelOpen()),
        then_send=m.ConnectionClose(reply_code=mf.ReplyCode.CHANNEL_ERROR),
    ),
    ScriptRule(Received(m.ChannelClose()), then_send=m.ChannelCloseOk()),
    ScriptRule(Received(m.BasicConsume()), then_send=m.BasicConsumeOk()),
)


def session_connected(stop_weight: int) -> se.Session:
    session = se.Session(
        se.SessionConfig(
            seed=SEED,
            stop_weight=stop_weight,
            weights=GREETING_WEIGHTS,
            script=SCRIPTED_REPLIES,
        )
    )
    session.on_connected()
    return session


async def poll_until(
    predicate: Callable[[], bool], timeout_s: float = 2.0, interval_s: float = 0.005
) -> None:
    async with asyncio.timeout(timeout_s):
        while not predicate():  # noqa: ASYNC110
            await asyncio.sleep(interval_s)
