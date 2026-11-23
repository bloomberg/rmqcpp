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
from collections import deque
from collections.abc import AsyncIterator, Iterator
from dataclasses import dataclass, field
from enum import StrEnum
from itertools import islice

from ..model import methods as m

HARD_MAX_TRANSCRIPT_LINES = 1_000_000
DEFAULT_TRANSCRIPT_LINES_MAX = HARD_MAX_TRANSCRIPT_LINES


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


class TranscriptWatchLossy(Exception):
    pass


@dataclass(slots=True)
class Transcript:
    lines: deque[TranscriptLine] = field(init=False)
    lines_max: int = DEFAULT_TRANSCRIPT_LINES_MAX
    got_line: asyncio.Event = field(default_factory=asyncio.Event, init=False)
    finished: bool = field(default=False, init=False)

    # sequence number (does not index into the ring buffer)
    next_index: int = field(default=0, init=False)

    def __post_init__(self) -> None:
        self.lines = deque(maxlen=self.lines_max)

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
                index=self.next_index,
                micros=micros,
                epoch_micros=epoch_micros,
                event=event,
                method=method,
                detail=detail,
                wire_bytes=wire_bytes,
            )
        )
        self.next_index += 1

        self.got_line.set()
        self.got_line.clear()

    def finish(self) -> None:
        self.finished = True
        self.got_line.set()

    def lines_after(self, index: int) -> Iterator[TranscriptLine]:
        count = self.next_index - index
        count = max(0, min(count, len(self.lines)))  # clamp
        reverse_iter = islice(reversed(self.lines), count)

        return reversed(list(reverse_iter))

    async def watch(self) -> AsyncIterator[TranscriptLine]:
        index = self.next_index - len(self.lines)

        while index < self.next_index or not self.finished:
            # ahead?
            if index == self.next_index:
                await self.got_line.wait()
            # behind?
            elif self.next_index - index <= len(self.lines):
                for line in self.lines_after(index):
                    index = line.index + 1
                    yield line
            # far behind?
            else:
                first_index = self.next_index - len(self.lines)
                raise TranscriptWatchLossy(
                    f"dropped lines {index}..{first_index - 1}; consume faster or allow a longer transcript"
                )
