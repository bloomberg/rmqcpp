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

import textwrap
from bisect import bisect_left
from typing import cast

from google.protobuf import text_format
from google.protobuf.message import Message

from ..v1 import imposter_pb2 as pb


class Transcript(tuple[pb.TranscriptLine, ...]):
    __slots__ = ()

    def __str__(self) -> str:
        return format_transcript(self)


def format_direction_arrow(event: pb.TranscriptEvent.ValueType) -> str:
    EGRESS_ARROW = "<--"  # pretend client on the left, server on the right
    INGRESS_ARROW = "-->"
    NO_ARROW = "   "

    assert len(EGRESS_ARROW) == len(INGRESS_ARROW)
    assert len(EGRESS_ARROW) == len(NO_ARROW)

    # all ingress
    if event in (
        pb.TRANSCRIPT_EVENT_PROTOCOL_HEADER,
        pb.TRANSCRIPT_EVENT_FRAME_HEARTBEAT,
        pb.TRANSCRIPT_EVENT_FRAME_MALFORMED,
        pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
        pb.TRANSCRIPT_EVENT_METHOD_UNREPRESENTABLE,
    ):
        return INGRESS_ARROW

    # all egress
    if event in (pb.TRANSCRIPT_EVENT_METHOD_EGRESS,):
        return EGRESS_ARROW

    return NO_ARROW


def format_event_name(event: pb.TranscriptEvent.ValueType) -> str:
    return pb.TranscriptEvent.Name(event).removeprefix("TRANSCRIPT_EVENT_")


def proto_without_empty_submessages(message: Message) -> Message:
    without_empties = type(message)()
    without_empties.CopyFrom(message)

    for field, value in message.ListFields():
        is_submessage = field.message_type is not None and not field.is_repeated

        if is_submessage and not value.ListFields():
            without_empties.ClearField(field.name)

    return without_empties


def format_proto_message(message: Message) -> str:
    return text_format.MessageToString(
        proto_without_empty_submessages(message), as_one_line=True
    )


def format_line_name(line: pb.TranscriptLine) -> str:
    return line.method.WhichOneof("kind") or format_event_name(line.event)


W_EVENT_NAME = max(
    len(format_event_name(event)) for event in pb.TranscriptEvent.values()
)
W_METHOD_KIND = max(
    len(field.name) for field in pb.Method.DESCRIPTOR.oneofs_by_name["kind"].fields
)
W_LINE_NAME = max(W_EVENT_NAME, W_METHOD_KIND)
W_METHOD_FIELDS = 100


def transcript_line_method(line: pb.TranscriptLine) -> Message | None:
    if (kind := line.method.WhichOneof("kind")) is not None:
        return cast(Message, getattr(line.method, kind))
    return None


def format_transcript_line(line: pb.TranscriptLine) -> str:
    method = transcript_line_method(line)
    method_fields = (
        format_proto_message(method) if method is not None else ""  # not a method
    )
    return (
        f"{line.index:>6} "
        f"{line.micros / 1_000_000:>10.6f} "
        f"{format_direction_arrow(line.event)} "
        f"{format_line_name(line):<{W_LINE_NAME}} "
        f"{textwrap.shorten(method_fields, W_METHOD_FIELDS)} {line.detail}"
    ).rstrip()


def format_transcript(transcript: Transcript) -> str:
    return "\n".join(map(format_transcript_line, transcript))


def format_transcript_around(
    transcript: Transcript,
    line_index: int | None,
    n_lines: int = 50,
) -> str:
    # N.b. line_index doesn't necessarily correspond to a list index
    # because the transcript can be truncated
    midpoint = (
        bisect_left(transcript, line_index, key=lambda line: line.index)
        if line_index is not None
        else len(transcript)
    )
    found_midpoint = (
        midpoint < len(transcript) and transcript[midpoint].index == line_index
    )

    start = max(0, min(midpoint - n_lines // 2, len(transcript) - n_lines))
    lines = transcript[start : start + n_lines]

    match (line_index, found_midpoint):
        case None, _:
            header = f"last {len(lines)} lines ({len(transcript)} total):"
        case _, True:
            header = (
                f"{len(lines)} lines around index {line_index} "
                f"({len(transcript)} total):"
            )
        case _, False:
            header = (
                f"{len(lines)} lines nearest index {line_index}, "
                f"which is not in the transcript ({len(transcript)} total):"
            )

    return "\n".join((header, *map(format_transcript_line, lines)))
