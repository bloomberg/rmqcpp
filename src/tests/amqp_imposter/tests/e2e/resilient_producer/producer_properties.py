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

from dataclasses import dataclass, replace
from functools import partial, reduce

from amqp_imposter.control.client import Transcript, pb

from ..property import Failure, Property, event_and_method


@dataclass(frozen=True, slots=True)
class UnconfirmedPublish:
    delivery_tag: int
    message_id: str
    returned: bool = False


@dataclass(frozen=True, slots=True)
class PublishState:
    next_delivery_tag: int = 1
    unconfirmed_publishes: tuple[UnconfirmedPublish, ...] = ()
    acked_message_ids: frozenset[str] = frozenset()
    unroutable_message_ids: frozenset[str] = frozenset()


def partition_confirmed_unconfirmed(
    unconfirmed_publishes: tuple[UnconfirmedPublish, ...],
    delivery_tag: int,
    multiple: bool,
) -> tuple[tuple[UnconfirmedPublish, ...], tuple[UnconfirmedPublish, ...]]:

    def is_confirmed(publish: UnconfirmedPublish) -> bool:
        return publish.delivery_tag == delivery_tag or (
            multiple and publish.delivery_tag < delivery_tag
        )

    return (
        tuple(p for p in unconfirmed_publishes if is_confirmed(p)),
        tuple(p for p in unconfirmed_publishes if not is_confirmed(p)),
    )


def fold_line_into_publish_state(
    state: PublishState, line: pb.TranscriptLine
) -> PublishState:
    match event_and_method(line):
        case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.ChannelOpen()):
            return PublishState(
                acked_message_ids=state.acked_message_ids,
                unroutable_message_ids=state.unroutable_message_ids,
            )

        case (pb.TRANSCRIPT_EVENT_METHOD_INGRESS, pb.BasicPublish() as publish):
            unconfirmed = UnconfirmedPublish(
                state.next_delivery_tag, publish.properties.message_id
            )
            return replace(
                state,
                next_delivery_tag=state.next_delivery_tag + 1,
                unconfirmed_publishes=(*state.unconfirmed_publishes, unconfirmed),
            )

        case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicAck() as ack):
            acked, unconfirmed_publishes = partition_confirmed_unconfirmed(
                state.unconfirmed_publishes, ack.delivery_tag, ack.multiple
            )
            return replace(
                state,
                unconfirmed_publishes=unconfirmed_publishes,
                acked_message_ids=state.acked_message_ids
                | {p.message_id for p in acked},
                unroutable_message_ids=state.unroutable_message_ids
                | {p.message_id for p in acked if p.returned},
            )

        case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicNack() as nack):
            _, unconfirmed_publishes = partition_confirmed_unconfirmed(
                state.unconfirmed_publishes, nack.delivery_tag, nack.multiple
            )
            return replace(state, unconfirmed_publishes=unconfirmed_publishes)

        case (pb.TRANSCRIPT_EVENT_METHOD_EGRESS, pb.BasicReturn() as basic_return):
            return replace(
                state,
                unconfirmed_publishes=tuple(
                    replace(p, returned=True)
                    if p.message_id == basic_return.properties.message_id
                    else p
                    for p in state.unconfirmed_publishes
                ),
            )

        case _:
            return state


def check_tracked_acks(
    transcript: Transcript, acked_message_ids: frozenset[str]
) -> Failure | None:
    state = reduce(fold_line_into_publish_state, transcript, PublishState())

    if missing := acked_message_ids - state.acked_message_ids:
        return Failure(f"Never acked: {', '.join(sorted(missing))}", None)

    return None


def check_tracked_unroutable(
    transcript: Transcript, unroutable_message_ids: frozenset[str]
) -> Failure | None:
    state = reduce(fold_line_into_publish_state, transcript, PublishState())

    if mismatched := unroutable_message_ids ^ state.unroutable_message_ids:
        return Failure(
            f"Tracked unroutable differs from observed on {', '.join(sorted(mismatched))}",
            None,
        )

    return None


def producer_properties(
    message_ids: frozenset[str], unroutable_message_ids: frozenset[str]
) -> tuple[Property, ...]:
    return (
        partial(
            # returned messages are also acked!
            check_tracked_acks,
            acked_message_ids=message_ids,
        ),
        partial(
            check_tracked_unroutable,
            unroutable_message_ids=unroutable_message_ids,
        ),
    )
