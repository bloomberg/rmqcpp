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

from dataclasses import dataclass
from functools import partial

from amqp_imposter.control.client import Transcript, pb
from amqp_imposter.control.client.transcript_util import (
    format_line_name,
)

from ..property import Failure, Property, event_and_method


@dataclass(frozen=True, slots=True)
class Exchange:
    name: str


@dataclass(frozen=True, slots=True)
class Queue:
    name: str


@dataclass(frozen=True, slots=True)
class Binding:
    exchange: str
    routing_key: str


type Declaration = Exchange | Queue | Binding
type Settlement = pb.BasicAck | pb.BasicNack | pb.BasicReject


def settled_delivery_tags(
    unsettled: frozenset[int], settlement: Settlement
) -> frozenset[int]:
    match settlement:
        case pb.BasicAck(multiple=True) | pb.BasicNack(multiple=True):
            return frozenset(
                t
                for t in unsettled
                if settlement.delivery_tag == 0 or t <= settlement.delivery_tag
            )

        case _:
            return unsettled & {settlement.delivery_tag}


def check_deliveries_acknowledged(
    transcript: Transcript, prefetch_count: int
) -> Failure | None:
    unsettled: frozenset[int] = frozenset()

    for line in transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ChannelOpen()):
                unsettled = frozenset()

            case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicDeliver() as deliver):
                unsettled |= {deliver.delivery_tag}

            case (
                pb.TRANSCRIPT_EVENT_METHOD_INGRESS,
                pb.BasicAck() | pb.BasicNack() | pb.BasicReject() as settlement,
            ):
                settled = settled_delivery_tags(unsettled, settlement)
                if not settled:
                    return Failure(
                        f"Settled delivery tag {settlement.delivery_tag}, "
                        f"which is not awaiting settlement on this channel",
                        line.index,
                    )

                unsettled -= settled

    if len(unsettled) > prefetch_count:
        return Failure(
            f"Left {len(unsettled)} deliveries unsettled: expected <={prefetch_count}",
            None,
        )

    return None


def check_after_teardown_reconsume(
    transcript: Transcript, deliveries: int
) -> Failure | None:
    teardown: pb.TranscriptLine | None = None
    delivered = 0

    for line in transcript:
        if delivered >= deliveries:
            break

        match event_and_method(line):
            case (
                pb.TRANSCRIPT_EVENT_METHOD_EGRESS,
                pb.ConnectionClose() | pb.ChannelClose() | pb.BasicCancel(),
            ):
                teardown = line

            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicConsume()):
                teardown = None

            case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicDeliver()):
                delivered += 1

    if teardown is not None:
        return Failure(
            f"Never consumed again after {format_line_name(teardown)}",
            teardown.index,
        )

    return None


def check_before_consume_declare(
    transcript: Transcript, topology: frozenset[Declaration]
) -> Failure | None:
    declared: set[Declaration] = set()
    declared_queue_names: set[str] = set()

    for line in transcript:
        match event_and_method(line):
            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ChannelOpen()) | (
                pb.TRANSCRIPT_EVENT_METHOD_EGRESS,
                pb.BasicCancel(),
            ):
                declared.clear()
                declared_queue_names.clear()

            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ExchangeDeclare() as declare):
                declared.add(Exchange(declare.exchange))

            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.QueueDeclare() as declare):
                declared.add(Queue(declare.queue))

            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.QueueBind() as bind):
                declared.add(Binding(bind.exchange, bind.routing_key))

            case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.QueueDeclareOk() as declare_ok):
                declared_queue_names.add(declare_ok.queue)

            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicConsume()) if (
                missing := topology - declared
            ):
                return Failure(
                    f"Consumed before declaring {', '.join(sorted(map(repr, missing)))}",
                    line.index,
                )

            case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicConsume() as consume) if (
                consume.queue not in declared_queue_names
            ):
                return Failure(
                    f"Consumed from {consume.queue!r}, "
                    f"which was not declared on this channel",
                    line.index,
                )

    return None


def properties(
    prefetch_count: int, deliveries: int, topology: frozenset[Declaration]
) -> tuple[Property, ...]:
    return (
        partial(check_deliveries_acknowledged, prefetch_count=prefetch_count),
        partial(check_after_teardown_reconsume, deliveries=deliveries),
        partial(check_before_consume_declare, topology=topology),
    )
