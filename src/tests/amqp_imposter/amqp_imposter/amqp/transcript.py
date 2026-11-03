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

from collections import deque
from dataclasses import dataclass, field
from enum import StrEnum

from ..model import methods as m

HARD_MAX_TRANSCRIPT_LINES = 1_000_000
DEFAULT_MAX_TRANSCRIPT_LINES = HARD_MAX_TRANSCRIPT_LINES


class TranscriptEvent(StrEnum):
    SESSION_STARTED = "SESSION_STARTED"
    TCP_CONNECTED = "TCP_CONNECTED"
    TCP_REJECTED = "TCP_REJECTED"
    TCP_DOWN = "TCP_DOWN"
    IDLE_TIMEOUT = "IDLE_TIMEOUT"
    PROTOCOL_HEADER = "PROTOCOL_HEADER"
    FRAME_MALFORMED = "FRAME_MALFORMED"
    FRAME_HEARTBEAT = "FRAME_HEARTBEAT"
    METHOD_INGRESS = "METHOD_INGRESS"
    METHOD_EGRESS = "METHOD_EGRESS"
    METHOD_UNREPRESENTABLE = "METHOD_UNREPRESENTABLE"


@dataclass(frozen=True, slots=True)
class TranscriptLine:
    index: int
    micros: int
    epoch_micros: int
    event: TranscriptEvent
    method: m.Method | None = None
    detail: str = ""
    wire_bytes: bytes = b""


@dataclass(slots=True)
class Transcript:
    max_lines: int = DEFAULT_MAX_TRANSCRIPT_LINES
    lines: deque[TranscriptLine] = field(init=False)
    recorded: int = field(default=0, init=False)

    def __post_init__(self) -> None:
        self.lines = deque(maxlen=self.max_lines)

    def add(
        self,
        micros: int,
        epoch_micros: int,
        event: TranscriptEvent,
        method: m.Method | None = None,
        detail: str = "",
        wire_bytes: bytes = b"",
    ) -> None:
        self.lines.append(
            TranscriptLine(
                index=self.recorded,
                micros=micros,
                epoch_micros=epoch_micros,
                event=event,
                method=method,
                detail=detail,
                wire_bytes=wire_bytes,
            )
        )
        self.recorded += 1
