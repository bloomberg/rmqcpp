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

from collections.abc import Iterator
from itertools import islice

from amqp_imposter.control.client import Transcript, pb

from .prop_general import (
    PropertyViolation,
    check_recovers_within,
    check_stays_stopped,
    client_stop_index,
    event_and_method,
    matches_event_method,
    settled_tags,
)
from .runner import ImposterCapture


def ends_channel(line: pb.TranscriptLine) -> bool:
    return (
        line.event in (pb.TRANSCRIPT_EVENT_TCP_CONNECTED, pb.TRANSCRIPT_EVENT_TCP_DOWN)
        or matches_event_method(
            line,
            pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
            pb.ChannelOpen,
            pb.ChannelClose,
            pb.ConnectionClose,
        )
        or matches_event_method(
            line, pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.ChannelClose, pb.ConnectionClose
        )
    )


def settled_publishes_by_line(
    transcript: Transcript,
) -> Iterator[tuple[pb.TranscriptLine, list[bytes]]]:
    unconfirmed_publishes: dict[int, bytes] = {}
    next_delivery_tag: int | None = None

    for line in transcript:
        if ends_channel(line):
            unconfirmed_publishes = {}
            next_delivery_tag = None

        settled_publishes: list[bytes] = []
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ConfirmSelect()) if (
                next_delivery_tag is None
            ):
                next_delivery_tag = 1
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicPublish() as publish) if (
                next_delivery_tag is not None
            ):
                # N.b. perftest generates unique message bodies
                unconfirmed_publishes[next_delivery_tag] = publish.SerializeToString(
                    deterministic=True
                )
                next_delivery_tag += 1
            case (
                pb.TRANSCRIPT_EVENT_METHOD_EGRESS,
                pb.BasicAck() | pb.BasicNack() as confirm,
            ):
                settled_publishes = [
                    unconfirmed_publishes.pop(tag)
                    for tag in sorted(
                        settled_tags(
                            frozenset(unconfirmed_publishes),
                            confirm.delivery_tag,
                            confirm.multiple,
                        )
                    )
                ]

        yield line, settled_publishes


def check_confirm_select_before_publishing(
    capture: ImposterCapture,
) -> PropertyViolation | None:
    confirming = False

    for line in capture.transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ChannelOpen()):
                confirming = False
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ConfirmSelect()):
                confirming = True
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicPublish()) if (
                not confirming
            ):
                return PropertyViolation(
                    "Missed Confirm.Select before publishing", line.index
                )

    return None


def check_expected_publishes(
    capture: ImposterCapture,
    exchange: str,
    routing_key: str,
    persistent: bool,
    body_size: int,
) -> PropertyViolation | None:
    for line in capture.transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicPublish() as publish) if (
                mismatches := [
                    f"{field}={actual!r}, expected {expected!r}"
                    for field, actual, expected in (
                        ("exchange", publish.exchange, exchange),
                        ("routing_key", publish.routing_key, routing_key),
                        ("body_size", len(publish.body), body_size),
                        (
                            "persistent",
                            publish.properties.delivery_mode == 2,
                            persistent,
                        ),
                    )
                    if actual != expected
                ]
            ):
                return PropertyViolation(
                    f"Published with {'; '.join(mismatches)}", line.index
                )

    return None


def check_max_unconfirmed(
    capture: ImposterCapture, max_unconfirmed: int
) -> PropertyViolation | None:
    n_unconfirmed = 0

    for line, settled_publish in settled_publishes_by_line(capture.transcript):
        if ends_channel(line):
            n_unconfirmed = 0

        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicAck() | pb.BasicNack()):
                n_unconfirmed -= len(settled_publish)
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicPublish()):
                n_unconfirmed += 1
                if n_unconfirmed > max_unconfirmed:
                    return PropertyViolation(
                        f"Published with {n_unconfirmed} messages unconfirmed: "
                        f"expected <={max_unconfirmed}",
                        line.index,
                    )

    return None


def check_message_count(
    capture: ImposterCapture, messages: int
) -> PropertyViolation | None:
    first_publish_indices: dict[bytes, int] = {}
    for line in capture.transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicPublish() as publish):
                first_publish_indices.setdefault(
                    publish.SerializeToString(deterministic=True), line.index
                )

    if len(first_publish_indices) != messages:
        return PropertyViolation(
            f"Published {len(first_publish_indices)} distinct messages: "
            f"expected {messages}",
            next(islice(first_publish_indices.values(), messages, None), None),
        )

    return None


def check_every_publish_settled(capture: ImposterCapture) -> PropertyViolation | None:
    first_publish_indices: dict[bytes, int] = {}
    settled_publishes: set[bytes] = set()

    for line, settled_publish in settled_publishes_by_line(capture.transcript):
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicPublish() as publish):
                first_publish_indices.setdefault(
                    publish.SerializeToString(deterministic=True), line.index
                )
        settled_publishes.update(settled_publish)

    if unsettled_publishes := [
        i_publish
        for message, i_publish in first_publish_indices.items()
        if message not in settled_publishes
    ]:
        return PropertyViolation(
            f"{len(unsettled_publishes)} of {len(first_publish_indices)} messages never settled",
            unsettled_publishes[0],
        )

    return None


def producer_stop_index(transcript: Transcript, messages: int) -> int | None:
    # With perftest, ack and nack both mean we're done with the message
    # So both count towards the stop target
    settled_messages: set[bytes] = set()
    first_settle_indices: list[int] = []

    for line, settled in settled_publishes_by_line(transcript):
        for message in settled:
            if message not in settled_messages:
                settled_messages.add(message)
                first_settle_indices.append(line.index)

    return client_stop_index(transcript, first_settle_indices, messages)


def check_producer_stays_stopped(
    capture: ImposterCapture, messages: int
) -> PropertyViolation | None:
    return check_stays_stopped(
        capture.transcript, producer_stop_index(capture.transcript, messages)
    )


def check_producer_recovers_within(
    capture: ImposterCapture, messages: int
) -> PropertyViolation | None:
    return check_recovers_within(
        capture.transcript, producer_stop_index(capture.transcript, messages)
    )
