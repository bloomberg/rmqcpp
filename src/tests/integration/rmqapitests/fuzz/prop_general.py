"""Copyright 2026 Bloomberg Finance L.P.
SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
from __future__ import annotations

from collections import defaultdict
from collections.abc import Callable, Iterable, Iterator
from copy import deepcopy
from dataclasses import dataclass
from itertools import islice

import pytest
from amqp_imposter.control.client import (
    FORCED_REPLY_TEXT,
    Transcript,
    format_transcript_around,
    pb,
    transcript_line_method,
)
from amqp_imposter.control.client.transcript_util import (
    format_line_name,
)
from google.protobuf.message import Message

from .runner import ImposterCapture


@dataclass(frozen=True, slots=True)
class PropertyViolation:
    reason: str
    i_line: int | None


type Property = Callable[[ImposterCapture], PropertyViolation | None]


# N.b. the imposter supports one channel
# If that changes, change to track by channel number, e.g. return it here
def event_and_method(
    line: pb.TranscriptLine,
) -> tuple[pb.TranscriptEvent.ValueType, Message] | None:
    if (method := transcript_line_method(line)) is not None:
        return line.event, method
    return None


def matches_event_method(
    line: pb.TranscriptLine,
    event: pb.TranscriptEvent.ValueType,
    *methods: type[Message],
) -> bool:
    match event_and_method(line):
        case (line_event, method):
            return line_event == event and isinstance(method, methods)
        case None:
            return False


def settled_tags(
    unsettled: frozenset[int], delivery_tag: int, multiple: bool
) -> frozenset[int]:
    if multiple:
        return frozenset(
            tag for tag in unsettled if delivery_tag == 0 or tag <= delivery_tag
        )
    return unsettled & {delivery_tag}


def check_protocol_legal(capture: ImposterCapture) -> PropertyViolation | None:
    for line in capture.transcript:
        match line.event:
            case pb.TRANSCRIPT_EVENT_METHOD_UNREPRESENTABLE:
                return PropertyViolation(
                    f"Imposter refused to continue after: {format_line_name(line)}, {line.detail}",
                    line.index,
                )
            case pb.TRANSCRIPT_EVENT_TCP_REJECTED:
                return PropertyViolation(
                    f"Imposter rejected connection: {line.detail}", line.index
                )
            case pb.TRANSCRIPT_EVENT_FRAME_MALFORMED:
                return PropertyViolation(f"Malformed frame: {line.detail}", line.index)

        match event_and_method(line):
            case (
                pb.TRANSCRIPT_EVENT_METHOD_EGRESS,
                pb.ConnectionClose() | pb.ChannelClose() as close,
            ) if close.reply_text == FORCED_REPLY_TEXT:
                return PropertyViolation(
                    f"Forced AMQP error: {format_line_name(line)}", line.index
                )

    return None


type TopologyMethod = pb.ExchangeDeclare | pb.QueueDeclare | pb.QueueBind


def topology_name(method: TopologyMethod) -> tuple[str, ...]:
    match method:
        case pb.ExchangeDeclare(exchange=exchange):
            return ("exchange", exchange)
        case pb.QueueDeclare(queue=queue):
            return ("queue", queue)
        case pb.QueueBind(queue=queue, exchange=exchange, routing_key=routing_key):
            return ("binding", queue, exchange, routing_key)


def check_declarations_before_work(
    capture: ImposterCapture, required: Iterable[TopologyMethod]
) -> PropertyViolation | None:
    required_names = set(map(topology_name, required))
    declared_names: set[tuple[str, ...]] = set()

    for line in capture.transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ChannelOpen()) | (
                pb.TRANSCRIPT_EVENT_METHOD_EGRESS,
                pb.BasicCancel(),
            ):
                declared_names = set()
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.ExchangeDeclare() | pb.QueueDeclare() | pb.QueueBind() as method,
            ):
                declared_names.add(topology_name(method))
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.BasicConsume() | pb.BasicPublish(),
            ) if missing := required_names - declared_names:
                return PropertyViolation(
                    f"Missed topology declarations before {format_line_name(line)}: "
                    f"{', '.join(sorted(map(repr, missing)))}",
                    line.index,
                )

    return None


def declaration_identity(
    declare: pb.ExchangeDeclare | pb.QueueDeclare,
) -> pb.ExchangeDeclare | pb.QueueDeclare:
    identity = deepcopy(declare)
    identity.ClearField("passive")
    identity.ClearField("no_wait")
    if not identity.arguments.entries:
        identity.ClearField("arguments")

    return identity


def field_mismatches(actual: Message, expected: Message) -> list[str]:
    return [
        f"{field.name}={getattr(actual, field.name)!r}, "
        f"expected {getattr(expected, field.name)!r}"
        for field in actual.DESCRIPTOR.fields
        if getattr(actual, field.name) != getattr(expected, field.name)
    ]


def check_expected_declarations(
    capture: ImposterCapture, expected: Iterable[pb.ExchangeDeclare | pb.QueueDeclare]
) -> PropertyViolation | None:
    expected_by_name = {
        topology_name(declare): declaration_identity(declare) for declare in expected
    }

    for line in capture.transcript:
        match event_and_method(line):
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.ExchangeDeclare(passive=False)
                | pb.QueueDeclare(passive=False) as declare,
            ):
                name = topology_name(declare)
                identity = declaration_identity(declare)
                match expected_by_name.get(name):
                    case None:
                        return PropertyViolation(
                            f"Unexpected declaration of {name}", line.index
                        )
                    case expected_identity if identity != expected_identity:
                        return PropertyViolation(
                            f"Declared {name} with "
                            f"{'; '.join(field_mismatches(identity, expected_identity))}",
                            line.index,
                        )

    return None


def client_shutdown_indices(transcript: Transcript) -> Iterator[int]:
    # track methods to be ignored as they are continuations of the shutdown sequence
    # so we shouldn't mistake them as having initiated it!
    to_ignore: frozenset[type[Message]] = frozenset()

    for line in transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicCancel()):
                to_ignore |= {pb.ChannelClose, pb.ConnectionClose}
            case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.ChannelClose()):
                to_ignore |= {pb.ConnectionClose}
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.ChannelOpen() | pb.BasicConsume(),
            ):
                to_ignore = frozenset()
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.ChannelClose() | pb.ConnectionClose() as close,
            ) if type(close) in to_ignore:
                to_ignore -= {type(close)}
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.BasicCancel() | pb.ChannelClose() | pb.ConnectionClose(),
            ):
                yield line.index


def client_stop_index(
    transcript: Transcript, completion_indices: Iterable[int], messages: int
) -> int | None:
    final_completion = islice(completion_indices, messages - 1, messages)
    return min((*final_completion, *client_shutdown_indices(transcript)), default=None)


def check_stays_stopped(
    transcript: Transcript, i_stop: int | None
) -> PropertyViolation | None:
    if i_stop is None:
        return None

    for line in transcript:
        restarted = (
            line.event == pb.TRANSCRIPT_EVENT_TCP_CONNECTED
            or matches_event_method(
                line,
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.ChannelOpen,
                pb.BasicConsume,
                pb.ConfirmSelect,
                pb.BasicPublish,
            )
        )
        if restarted and line.index > i_stop:
            return PropertyViolation(
                f"{format_line_name(line)} after client shutdown at index {i_stop}",
                line.index,
            )

    return None


def check_recovers_within(
    transcript: Transcript, i_stop: int | None, max_secs: float = 120
) -> PropertyViolation | None:
    teardown_line: pb.TranscriptLine | None = None

    for line in transcript:
        if i_stop is not None and line.index > i_stop:
            return None
        if line.event == pb.TRANSCRIPT_EVENT_TCP_DOWN or matches_event_method(
            line,
            pb.TRANSCRIPT_EVENT_METHOD_EGRESS,
            pb.BasicCancel,
            pb.ChannelClose,
            pb.ConnectionClose,
        ):
            teardown_line = line
        elif teardown_line is not None and matches_event_method(
            line, pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicConsume, pb.BasicPublish
        ):
            secs = (line.micros - teardown_line.micros) / 1_000_000
            if secs > max_secs:
                return PropertyViolation(
                    f"Resumed {secs:.1f} sec after {format_line_name(teardown_line)}: "
                    f"expected <={max_secs:.1f} sec",
                    line.index,
                )
            teardown_line = None

    if teardown_line is not None:
        return PropertyViolation(
            f"Never resumed after {format_line_name(teardown_line)}",
            teardown_line.index,
        )
    return None


def check_connections_closed(capture: ImposterCapture) -> PropertyViolation | None:
    i_openok: int | None = None

    for line in capture.transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.ConnectionOpenOk()):
                i_openok = line.index
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS | pb.TRANSCRIPT_EVENT_METHOD_EGRESS,
                pb.ConnectionClose(),
            ):
                i_openok = None
            case _ if (
                line.event == pb.TRANSCRIPT_EVENT_TCP_DOWN and i_openok is not None
            ):
                # imposter exited gracefully, if we don't have a close then we've leaked the connection
                break

    if i_openok is not None:
        return PropertyViolation(
            f"Connection leaked or hung: open_ok at index {i_openok} was never closed",
            i_openok,
        )
    return None


def check_open_backoff(
    capture: ImposterCapture,
    window_opens: int = 10,
    min_window_secs: float = 5,  # N opens should be spread over at least T seconds
) -> PropertyViolation | None:
    for open_kind in (pb.ConnectionOpen, pb.ChannelOpen):
        opens = [
            line
            for line in capture.transcript
            if matches_event_method(line, pb.TRANSCRIPT_EVENT_METHOD_INGRESS, open_kind)
        ]

        for window_start, window_end in zip(opens, opens[window_opens:], strict=False):
            window_secs = (window_end.micros - window_start.micros) / 1_000_000
            if window_secs < min_window_secs:
                return PropertyViolation(
                    f">{window_opens} of {format_line_name(window_end)} in {window_secs:.1f} sec: "
                    f"expected >={min_window_secs:.1f} sec",
                    window_end.index,
                )

    return None


def check_clean_exit(capture: ImposterCapture) -> PropertyViolation | None:
    match capture.rc:
        case 0:
            return None
        case None:
            return PropertyViolation("Client did not exit in time", None)
        case rc:
            return PropertyViolation(f"Client exited ungracefully: rc={rc}", None)


def assert_properties(capture: ImposterCapture, properties: Iterable[Property]) -> None:
    # embed the clean exit check
    results = (
        check_clean_exit(capture),
        *(prop(capture) for prop in properties),
    )

    reasons_by_line: defaultdict[int | None, list[str]] = defaultdict(list)
    for failure in filter(None, results):
        reasons_by_line[failure.i_line].append(failure.reason)

    if reasons_by_line:
        pytest.fail(
            "\n\n".join(
                "\n".join(
                    (*reasons, format_transcript_around(capture.transcript, i_line))
                )
                for i_line, reasons in reasons_by_line.items()
            ),
            pytrace=False,
        )
