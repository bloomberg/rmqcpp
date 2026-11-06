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

from amqp_imposter.control.client import Transcript, pb
from amqp_imposter.control.client.transcript_util import (
    format_line_name,
    format_proto_message,
)

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


def delivery_ack_indices(transcript: Transcript) -> Iterator[int]:
    unsettled: frozenset[int] = frozenset()

    for line in transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ChannelOpen()) | (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS | pb.TRANSCRIPT_EVENT_METHOD_EGRESS,
                pb.ChannelClose() | pb.ConnectionClose(),
            ):
                unsettled = frozenset()
            case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicDeliver(delivery_tag=tag)):
                unsettled |= {tag}
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.BasicAck(delivery_tag=tag, multiple=multiple),
            ):
                acked = settled_tags(unsettled, tag, multiple)
                yield from [line.index] * len(acked)
                unsettled -= acked
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.BasicNack(delivery_tag=tag, multiple=multiple),
            ):
                unsettled -= settled_tags(unsettled, tag, multiple)
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicReject(delivery_tag=tag)):
                unsettled -= {tag}


def check_expected_requeue(
    capture: ImposterCapture, requeue: bool
) -> PropertyViolation | None:
    for line in capture.transcript:
        match event_and_method(line):
            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                (pb.BasicNack() | pb.BasicReject()) as method,
            ) if method.requeue != requeue:
                return PropertyViolation(
                    f"{format_line_name(line)} with requeue={method.requeue}",
                    line.index,
                )

    return None


def check_consumes(
    capture: ImposterCapture, queue: str, priority: int
) -> PropertyViolation | None:
    expected = (queue, False, pb.AmqpValue(long=priority))

    for line in capture.transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicConsume() as consume) if (
                consume.queue,
                consume.no_ack,
                consume.arguments.entries.get("x-priority"),
            ) != expected:
                # TODO we could generalise this check if we expose more in perftest
                return PropertyViolation(
                    f"Consumed {format_proto_message(consume)!r}: "
                    f"expected queue={queue!r}, no_ack=false, x-priority={priority}",
                    line.index,
                )

    return None


def check_qos_before_consuming(
    capture: ImposterCapture, prefetch_count: int
) -> PropertyViolation | None:
    expected_qos = pb.BasicQos(prefetch_count=prefetch_count)
    prior_qos: pb.BasicQos | None = None

    for line in capture.transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ChannelOpen()):
                prior_qos = None
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicQos() as qos):
                prior_qos = qos
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicConsume()) if (
                prior_qos != expected_qos
            ):
                return PropertyViolation(
                    f"Missed QoS before consuming: {format_proto_message(expected_qos)!r}",
                    line.index,
                )

    return None


def check_delivery_count(
    capture: ImposterCapture, messages: int
) -> PropertyViolation | None:
    deliveries = sum(
        matches_event_method(line, pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicDeliver)
        for line in capture.transcript
    )

    if deliveries < messages:
        return PropertyViolation(
            f"Got {deliveries} deliveries: expected >={messages}", None
        )

    return None


def check_ack_count(
    capture: ImposterCapture, minimum: int, maximum: int
) -> PropertyViolation | None:
    ack_indices = tuple(delivery_ack_indices(capture.transcript))

    if len(ack_indices) < minimum:
        return PropertyViolation(
            f"Acked {len(ack_indices)} deliveries: expected >={minimum}", None
        )
    if len(ack_indices) > maximum:
        return PropertyViolation(
            f"Acked {len(ack_indices)} deliveries: expected <={maximum}",
            ack_indices[maximum],
        )

    return None


def check_consumer_stays_stopped(
    capture: ImposterCapture, messages: int
) -> PropertyViolation | None:
    i_stop = client_stop_index(
        capture.transcript, delivery_ack_indices(capture.transcript), messages
    )
    return check_stays_stopped(capture.transcript, i_stop)


def check_consumer_recovers_within(
    capture: ImposterCapture, messages: int
) -> PropertyViolation | None:
    delivery_indices = (
        line.index
        for line in capture.transcript
        if matches_event_method(
            line, pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicDeliver
        )
    )
    i_stop = client_stop_index(capture.transcript, delivery_indices, messages)
    return check_recovers_within(capture.transcript, i_stop)
