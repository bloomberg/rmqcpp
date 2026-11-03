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

import time
from dataclasses import dataclass, field

from ..model import generators as g
from ..model import method_fields as mf
from ..model import methods as m
from ..model import model_state as ms
from ..model.generators import DEFAULT_GEN_STATE
from ..model.method_sampling import DEFAULT_MAX_SENDS
from ..model.method_scripts import ScriptRule
from ..model.method_weights import (
    DEFAULT_WEIGHTS,
    STOP_WEIGHT,
    WeightedPattern,
)
from ..model.model_server import ModelServer
from ..model.negotiation_options import (
    DEFAULT_NEGOTIATION_OPTIONS,
    NegotiationOptions,
)
from . import method_framing as fr
from .transcript import (
    DEFAULT_MAX_TRANSCRIPT_LINES,
    Transcript,
    TranscriptEvent,
    TranscriptLine,
)

DEFAULT_IDLE_MILLIS = 5
DEFAULT_MAX_BUFFER = 16 * 1024 * 1024

GLOBAL_CHANNEL = 0

REASON_SIMULTANEOUS_CONNECTION = "a connection is already active"
REASON_DUPE_PROTOCOL_HEADER = "the protocol header was already given"
REASON_UNSUPPORTED_PROTOCOL = "only AMQP 0-9-1 is supported"
REASON_NOT_GREETED = "no protocol header was given"
REASON_CONNECTION_NOT_ON_GLOBAL = "a connection method must be on the global channel"
REASON_CHANNEL_ON_GLOBAL = "a channel method cannot be on the global channel"
REASON_SIMULTANOUS_CHANNELS = "another channel is already open"
REASON_UNREPRESENTABLE = "the model cannot represent this method"
REASON_NO_AMQP_CONNECTION = "the connection is closed"


@dataclass(frozen=True, slots=True)
class SessionConfig:
    seed: int = DEFAULT_GEN_STATE.seed
    negotiation_options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS
    script: tuple[ScriptRule, ...] = ()
    weights: tuple[WeightedPattern, ...] = DEFAULT_WEIGHTS
    idle_weights: tuple[WeightedPattern, ...] = ()
    idle_millis: int = DEFAULT_IDLE_MILLIS
    stop_weight: int = STOP_WEIGHT
    max_sends: int = DEFAULT_MAX_SENDS
    max_buffer: int = DEFAULT_MAX_BUFFER
    max_transcript_lines: int = DEFAULT_MAX_TRANSCRIPT_LINES


@dataclass(frozen=True, slots=True)
class Write:
    payload: bytes


@dataclass(frozen=True, slots=True)
class Drop:
    reason: str = ""


type SocketAction = Write | Drop

DEFAULT_SESSION_CONFIG = SessionConfig()


@dataclass(slots=True)
class Session:
    config: SessionConfig = DEFAULT_SESSION_CONFIG
    model: ModelServer = field(init=False)
    lines: Transcript = field(init=False)
    buffer: bytes = field(init=False, default=b"")
    channel: int | None = field(init=False, default=None)
    greeted: bool = field(init=False, default=False)
    total_ingress_methods: int = field(init=False, default=0)
    started_ns: int = field(init=False, default=0)

    def __post_init__(self) -> None:
        self.reset(self.config)

    def fresh_model(self, gen_state: g.GenState) -> ModelServer:
        return ModelServer(
            negotiation_options=self.config.negotiation_options, gen_state=gen_state
        )

    def transcribe(
        self,
        event: TranscriptEvent,
        method: m.Method | None = None,
        detail: str = "",
        wire_bytes: bytes = b"",
    ) -> None:
        self.lines.add(
            micros=(time.monotonic_ns() - self.started_ns) // 1_000,
            epoch_micros=time.time_ns() // 1_000,
            event=event,
            method=method,
            detail=detail,
            wire_bytes=wire_bytes,
        )

    def reset(self, config: SessionConfig | None = None) -> None:
        self.config = self.config if config is None else config
        self.model = self.fresh_model(g.GenState(self.config.seed))
        self.lines = Transcript(max_lines=self.config.max_transcript_lines)
        self.buffer = b""
        self.channel = None
        self.greeted = False
        self.started_ns = time.monotonic_ns()
        self.transcribe(TranscriptEvent.SESSION_STARTED)

    def transcript(self) -> tuple[TranscriptLine, ...]:
        return tuple(self.lines.lines)

    def is_greeted(self) -> bool:
        return self.greeted

    def on_connected(self) -> tuple[SocketAction, ...]:
        self.transcribe(TranscriptEvent.TCP_CONNECTED)
        return ()

    def on_rejected(self) -> tuple[SocketAction, ...]:
        self.transcribe(
            TranscriptEvent.TCP_REJECTED, detail=REASON_SIMULTANEOUS_CONNECTION
        )
        return (Drop(REASON_SIMULTANEOUS_CONNECTION),)

    def on_disconnected(self) -> tuple[SocketAction, ...]:
        self.transcribe(TranscriptEvent.TCP_DOWN)
        self.model = self.fresh_model(self.model.gen_state)
        self.buffer = b""
        self.channel = None
        self.greeted = False
        return ()

    def on_idle(self) -> tuple[SocketAction, ...]:
        methods = self.sample_method_burst(
            (*self.config.idle_weights, *self.config.weights)
        )
        if methods:
            # only record effectful idle timeouts in the transcript
            # if someone really wants to they can predict where the no-op timeouts are anyway
            self.transcribe(TranscriptEvent.IDLE_TIMEOUT)

        return self.egress(methods)

    def on_ingress(self, chunk: bytes) -> tuple[SocketAction, ...]:
        items, self.buffer = fr.decode(self.buffer + chunk, self.config.max_buffer)
        actions: list[SocketAction] = []

        for item in items:
            actions.extend(self.on_item(item))

            if any(isinstance(action, Drop) for action in actions):
                break

        return tuple(actions)

    def on_item(self, item: fr.Ingress) -> tuple[SocketAction, ...]:
        match item:
            case fr.ProtocolHeader(wire_bytes):
                return self.on_protocol_header(wire_bytes)
            case fr.Heartbeat():
                self.transcribe(TranscriptEvent.FRAME_HEARTBEAT)
                return ()
            case fr.Undecodable(reason):
                self.transcribe(TranscriptEvent.FRAME_MALFORMED, detail=reason)
                return (Drop(reason),)
            case fr.MethodOnChannel(channel, method):
                return self.on_method(channel, method)

    def on_protocol_header(self, wire_bytes: bytes) -> tuple[SocketAction, ...]:
        if self.greeted:
            self.transcribe(
                TranscriptEvent.METHOD_UNREPRESENTABLE,
                detail=REASON_DUPE_PROTOCOL_HEADER,
            )
            return (Drop(REASON_DUPE_PROTOCOL_HEADER),)

        if wire_bytes != fr.PROTOCOL_HEADER_091:
            self.transcribe(
                TranscriptEvent.METHOD_UNREPRESENTABLE,
                detail=REASON_UNSUPPORTED_PROTOCOL,
                wire_bytes=wire_bytes,
            )
            return (Write(fr.PROTOCOL_HEADER_091), Drop(REASON_UNSUPPORTED_PROTOCOL))

        self.transcribe(TranscriptEvent.PROTOCOL_HEADER, wire_bytes=wire_bytes)

        if self.model.receive_protocol_header() is not None:
            self.transcribe(
                TranscriptEvent.METHOD_UNREPRESENTABLE, detail=REASON_UNREPRESENTABLE
            )
            return (Drop(REASON_UNREPRESENTABLE),)

        self.greeted = True
        methods = self.sample_method_burst(self.config.weights)

        return self.egress(methods)

    def validate_channel(self, channel: int, method: m.Method) -> str | None:
        if not self.greeted:
            return REASON_NOT_GREETED

        if method.CLASS_ID == mf.ClassId.CONNECTION:
            if channel == GLOBAL_CHANNEL:
                return None
            else:
                return REASON_CONNECTION_NOT_ON_GLOBAL

        if channel == GLOBAL_CHANNEL:
            return REASON_CHANNEL_ON_GLOBAL

        if self.channel is not None and channel != self.channel:
            return REASON_SIMULTANOUS_CHANNELS

        return None

    def on_method(self, channel: int, method: m.Method) -> tuple[SocketAction, ...]:
        if (reason := self.validate_channel(channel, method)) is not None:
            self.transcribe(
                TranscriptEvent.METHOD_UNREPRESENTABLE,
                method=method,
                detail=reason,
            )
            return (Drop(reason),)

        match self.model.receive(method):
            case ms.Unrepresentable():
                self.transcribe(
                    TranscriptEvent.METHOD_UNREPRESENTABLE,
                    method=method,
                    detail=REASON_UNREPRESENTABLE,
                )
                return (Drop(REASON_UNREPRESENTABLE),)

            case concrete_method:
                self.transcribe(TranscriptEvent.METHOD_INGRESS, method=concrete_method)
                self.total_ingress_methods += 1

                if channel != GLOBAL_CHANNEL:
                    self.channel = channel

                methods = self.sample_method_burst(self.config.weights)
                actions = self.egress(methods)
                self.release_channel()

                return (
                    (*actions, Drop(REASON_NO_AMQP_CONNECTION))
                    if self.finished()
                    else actions
                )

    def finished(self) -> bool:
        return self.greeted and isinstance(
            self.model.model_state.connection_state, ms.ClosedConnectionState
        )

    def release_channel(self) -> None:
        match self.model.model_state.connection_state:
            case ms.OpenConnectionState(channel_state=ms.OpenChannelState()):
                return
            case ms.OpenConnectionState(channel_state=ms.ClosingChannelState()):
                return
            case _:
                self.channel = None

    def egress_channel(self, method: m.Method) -> int:
        if method.CLASS_ID == mf.ClassId.CONNECTION or self.channel is None:
            return GLOBAL_CHANNEL

        return self.channel

    def sample_method_burst(
        self, weighted_patterns: tuple[WeightedPattern, ...]
    ) -> list[m.Method]:
        return self.model.send_gens(
            self.config.script,
            weighted_patterns,
            self.config.stop_weight,
            self.config.max_sends,
        )

    def egress(self, methods: list[m.Method]) -> tuple[SocketAction, ...]:
        payload = b""

        for method in methods:
            encoded = fr.encode(
                fr.MethodOnChannel(self.egress_channel(method), method),
                self.config.negotiation_options.frame_max,
            )
            self.transcribe(
                TranscriptEvent.METHOD_EGRESS, method=method, wire_bytes=encoded
            )
            payload += encoded

        return (Write(payload),) if payload else ()
