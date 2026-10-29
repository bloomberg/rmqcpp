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

from collections.abc import Callable, Iterator
from dataclasses import dataclass, replace

from . import error_replies as er
from . import method_fields as mf
from . import methods as m
from .helpers.revealing_line import (
    Opaque,
    Revealed,
    RevealingLine,
    Transparent,
    is_opaque,
)
from .negotiation_options import DEFAULT_NEGOTIATION_OPTIONS, NegotiationOptions


@dataclass(frozen=True, slots=True)
class Unrepresentable:
    method: m.Method | None = None


@dataclass(frozen=True, slots=True)
class MessageState:
    exchange: str
    routing_key: str
    properties: mf.MessageProperties | None
    body: bytes


@dataclass(frozen=True, slots=True)
class UnreturnableRoutingState:
    pass


@dataclass(frozen=True, slots=True)
class ReturnableRoutingState:
    pass


@dataclass(frozen=True, slots=True)
class ReturnedRoutingState:
    pass


type RoutingState = (
    UnreturnableRoutingState | ReturnableRoutingState | ReturnedRoutingState
)


@dataclass(frozen=True, slots=True)
class PublishState:
    confirm_seq: int | None  # if not confirming
    routing_state: RoutingState
    message_state: MessageState


@dataclass(frozen=True, slots=True)
class DeliveryState:
    delivery_tag: int
    message_state: MessageState


@dataclass(frozen=True, slots=True)
class ActiveConsumerState:
    consumer_tag: str
    no_ack: bool


@dataclass(frozen=True, slots=True)
class CancelledConsumerState:
    consumer_tag: str


@dataclass(frozen=True, slots=True)
class NoConsumerState:
    pass


type ConsumerState = ActiveConsumerState | CancelledConsumerState | NoConsumerState


@dataclass(frozen=True, slots=True)
class NotConfirmingState:
    pass


@dataclass(frozen=True, slots=True)
class ConfirmingState:
    next_confirm_seq: int = 1


type ConfirmState = NotConfirmingState | ConfirmingState


@dataclass(frozen=True, slots=True)
class ClosedChannelState:
    pass


@dataclass(frozen=True, slots=True)
class OpenChannelState:
    confirm_state: ConfirmState = NotConfirmingState()
    consumer_state: ConsumerState = NoConsumerState()

    unacked_states: tuple[DeliveryState, ...] = ()
    unconfirmed_states: tuple[PublishState, ...] = ()  # confirmables and returnables

    prefetch_count: int = 0

    next_delivery_tag: int = 1


@dataclass(frozen=True, slots=True)
class ClosingChannelState:
    pass


type ChannelState = OpenChannelState | ClosedChannelState | ClosingChannelState


@dataclass(frozen=True, slots=True)
class ClosedConnectionState:
    pass


@dataclass(frozen=True, slots=True)
class GreetedConnectionState:
    pass


@dataclass(frozen=True, slots=True)
class StartedConnectionState:
    pass


@dataclass(frozen=True, slots=True)
class TunedConnectionState:
    pass


@dataclass(frozen=True, slots=True)
class OpenConnectionState:
    channel_state: ChannelState = ClosedChannelState()


@dataclass(frozen=True, slots=True)
class ClosingConnectionState:
    pass


type ConnectionState = (
    ClosedConnectionState
    | GreetedConnectionState
    | StartedConnectionState
    | TunedConnectionState
    | OpenConnectionState
    | ClosingConnectionState
)

INITIAL_CONNECTION_STATE = ClosedConnectionState()


@dataclass(frozen=True, slots=True)
class KnownReplyState:
    expected_methods: tuple[m.Method, ...]
    causing_method: m.Method | None = None


@dataclass(frozen=True, slots=True)
class DeferredPublishState:
    publish_state: PublishState


@dataclass(frozen=True, slots=True)
class DeferredReplyState:
    causing_method: m.BasicGet


type ReplyState = KnownReplyState | DeferredReplyState
type RevealedState = ReplyState | DeferredPublishState

EMPTY_LINE: RevealingLine[RevealedState] = RevealingLine()


@dataclass(frozen=True, slots=True)
class ModelState:
    connection_state: ConnectionState = INITIAL_CONNECTION_STATE

    # a line of RevealedStates; only what can be observed from the head is sendable
    line: RevealingLine[RevealedState] = EMPTY_LINE

    # requeued messages on standby; deliveries are always sendable
    requeued_message_states: tuple[MessageState, ...] = ()

    negotiation_options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS


def message_state(
    method: m.BasicDeliver | m.BasicGetOk | m.BasicPublish | m.BasicReturn,
) -> MessageState:
    exchange = mf.assert_concrete(method.exchange)
    routing_key = mf.assert_concrete(method.routing_key)
    properties = mf.assert_concrete(method.properties)
    body = mf.assert_concrete(method.body)

    return MessageState(
        exchange=exchange, routing_key=routing_key, properties=properties, body=body
    )


###   =========
###    Helpers
###   =========


def observable_replies(state: ModelState) -> tuple[tuple[int, ReplyState], ...]:
    observable, _ = state.line.split()
    return tuple(
        (i, revealed.item)
        for i, revealed in enumerate(observable)
        if isinstance(revealed.item, (KnownReplyState, DeferredReplyState))
    )


def observable_publishes(state: ModelState) -> tuple[PublishState, ...]:
    observable, _ = state.line.split()
    return filter_deferred_publishes(observable)


def awaits_consume_ok(state: ModelState) -> bool:
    return any(
        is_opaque(revealed)
        and isinstance(revealed.item, KnownReplyState)
        and isinstance(revealed.item.causing_method, m.BasicConsume)
        for revealed in state.line
    )


def has_pending_consume(state: ModelState) -> bool:
    return any(
        isinstance(revealed.item, KnownReplyState)
        and isinstance(revealed.item.causing_method, m.BasicConsume)
        for revealed in state.line
    )


def is_deferred_publish(revealed: Revealed[RevealedState]) -> bool:
    return isinstance(revealed.item, DeferredPublishState)


def filter_deferred_publishes(
    line: RevealingLine[RevealedState],
) -> tuple[PublishState, ...]:
    return tuple(
        item.publish_state
        for item in line.items()
        if isinstance(item, DeferredPublishState)
    )


def has_terminal_error(state: ModelState) -> bool:
    # check if there's a forced close anywhere in line
    # in our case it's both connection and channel errors (there's no multiplexing yet)
    return any(
        is_opaque(revealed)
        and isinstance(revealed.item, KnownReplyState)
        and all(
            isinstance(method, (m.ChannelClose, m.ConnectionClose))
            for method in revealed.item.expected_methods
        )
        for revealed in state.line
    )


def is_returnable(publish_state: PublishState) -> bool:
    return isinstance(publish_state.routing_state, ReturnableRoutingState)


def is_returned(publish_state: PublishState) -> bool:
    return isinstance(publish_state.routing_state, ReturnedRoutingState)


def is_confirmable(publish_state: PublishState) -> bool:
    return publish_state.confirm_seq is not None


def is_confirmed_by(
    publish_state: PublishState, confirm_seq: int, multiple: bool
) -> bool:
    if publish_state.confirm_seq is None:
        # no-confirm
        return False

    if multiple:
        return publish_state.confirm_seq <= confirm_seq

    return publish_state.confirm_seq == confirm_seq


def is_acknowledgeable(
    channel_state: OpenChannelState, delivery_tag: int, multiple: bool
) -> bool:
    if delivery_tag == 0:
        return multiple  # do not permit delivery-tag=0 with multiple=False

    return any(
        delivery.delivery_tag == delivery_tag
        for delivery in channel_state.unacked_states
    )


def has_prefetch_space(
    channel_state: OpenChannelState, consumer_state: ActiveConsumerState
) -> bool:
    return (
        consumer_state.no_ack
        or channel_state.prefetch_count == 0
        or len(channel_state.unacked_states) < channel_state.prefetch_count
    )


def maybe_make_confirm_seq(channel_state: OpenChannelState) -> int | None:
    match channel_state.confirm_state:
        case ConfirmingState(next_confirm_seq):
            return next_confirm_seq
        case NotConfirmingState():
            return None


def advanced_confirm_state(confirm_state: ConfirmState) -> ConfirmState:
    match confirm_state:
        case ConfirmingState(next_confirm_seq):
            return ConfirmingState(next_confirm_seq + 1)
        case NotConfirmingState():
            return confirm_state


def confirm_seqs(publish_states: tuple[PublishState, ...]) -> tuple[int, ...]:
    return tuple(
        publish_state.confirm_seq
        for publish_state in publish_states
        if publish_state.confirm_seq is not None
    )


def returned_confirm_seqs(publish_states: tuple[PublishState, ...]) -> frozenset[int]:
    return frozenset(
        publish_state.confirm_seq
        for publish_state in publish_states
        if publish_state.confirm_seq is not None and is_returned(publish_state)
    )


def owe_reply(
    state: ModelState,
    reply_method: m.Method,
    causing_method: m.Method | None = None,
) -> ModelState:
    reply_state = KnownReplyState((reply_method,), causing_method)
    line = state.line.push(Opaque(reply_state))
    state = replace(state, line=line)
    return state


def owe_optional_reply(state: ModelState, causing_method: m.Method) -> ModelState:
    to_schedule: Revealed[RevealedState] = Transparent(
        KnownReplyState((), causing_method)
    )
    match state.line.last():
        case Transparent(KnownReplyState((), last_causing_method)) if (
            # optimisation
            # we can merge identical error replies for different transparent causing methods
            # as we ne never need to correlate them back to the cause
            er.optional_error_replies(last_causing_method)
            == er.optional_error_replies(causing_method)
        ):
            line, _ = state.line.split(len(state.line) - 1)
            return replace(state, line=line.push(to_schedule))
        case _:
            return replace(state, line=state.line.push(to_schedule))


def owe_get_reply(state: ModelState, get: m.BasicGet) -> ModelState:
    state = replace(
        state, line=state.line.push(Opaque(DeferredReplyState(causing_method=get)))
    )
    return state


def owe_no_replies(state: ModelState) -> ModelState:
    state = replace(state, line=EMPTY_LINE)
    return state


def set_prefetch_count(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    prefetch_count: int,
) -> ModelState:
    channel_state = replace(channel_state, prefetch_count=prefetch_count)
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state


def owe_publish(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    publish_state: PublishState,
) -> ModelState:
    channel_state = replace(
        channel_state, confirm_state=advanced_confirm_state(channel_state.confirm_state)
    )
    connection_state = replace(connection_state, channel_state=channel_state)

    state = replace(state, connection_state=connection_state)

    if is_confirmable(publish_state) or is_returnable(publish_state):
        line = state.line.push(Transparent(DeferredPublishState(publish_state)))

        state = replace(state, line=line)
        state, _ = advance_line(state, connection_state, channel_state, 0)
        return state
    else:
        return state


def add_unconfirmed(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    publish_states: tuple[PublishState, ...],
) -> tuple[ModelState, OpenChannelState]:
    unconfirmed_states = (*channel_state.unconfirmed_states, *publish_states)

    channel_state = replace(
        channel_state,
        unconfirmed_states=unconfirmed_states,
    )
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state, channel_state


###   =========================================
###    Publish, delivery and reply bookkeeping
###   =========================================


def advance_line(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    n: int,
) -> tuple[ModelState, OpenChannelState]:
    retired, rest = state.line.split(n)
    # drain publish_states from head so they can move to unconfirmed
    drained, line = rest.split_while(is_deferred_publish)

    state = replace(state, line=line)
    return add_unconfirmed(
        state,
        connection_state,
        channel_state,
        filter_deferred_publishes(retired) + filter_deferred_publishes(drained),
    )


def publish_indices(
    line: RevealingLine[RevealedState], predicate: Callable[[PublishState], bool]
) -> Iterator[int]:
    return (
        i
        for i, revealed in enumerate(line)
        if isinstance(revealed.item, DeferredPublishState)
        and predicate(revealed.item.publish_state)
    )


def retire_reply(state: ModelState, i: int) -> ModelState:
    match connection_state := state.connection_state:
        case OpenConnectionState(channel_state=OpenChannelState() as channel_state):
            state, _ = advance_line(state, connection_state, channel_state, i + 1)
            return state
        case _:
            retired, line = state.line.split(i + 1)

            assert not filter_deferred_publishes(retired), (
                "deferred publishes in non-open channel"
            )
            return replace(state, line=line)


def maybe_track_delivery(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    no_ack: bool,
    delivery_state: DeliveryState,
) -> ModelState:
    next_delivery_tag = channel_state.next_delivery_tag + 1
    unacked_states = (
        channel_state.unacked_states
        if no_ack
        else (*channel_state.unacked_states, delivery_state)
    )

    channel_state = replace(
        channel_state,
        next_delivery_tag=next_delivery_tag,
        unacked_states=unacked_states,
    )
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state


def retire_requeued(state: ModelState, message: MessageState) -> ModelState:
    requeued_message_states = state.requeued_message_states

    # find a requeued mesage to retire
    for i, requeued in enumerate(requeued_message_states):
        if requeued == message:
            remaining = requeued_message_states[:i] + requeued_message_states[i + 1 :]
            return replace(state, requeued_message_states=remaining)

    return state


def return_unconfirmed(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    message: MessageState,
) -> ModelState:

    def correlate(publish_state: PublishState) -> bool:
        return is_returnable(publish_state) and publish_state.message_state == message

    n = (  # find the correlated publish
        0
        if any(map(correlate, channel_state.unconfirmed_states))
        else next((i + 1 for i in publish_indices(state.line, correlate)), 0)
    )
    state, channel_state = advance_line(state, connection_state, channel_state, n)

    unconfirmed_states = channel_state.unconfirmed_states
    i = next(
        (
            i
            for i, publish_state in enumerate(unconfirmed_states)
            if correlate(publish_state)
        ),
        None,
    )
    if i is None:
        return state

    publish_state = unconfirmed_states[i]

    if is_confirmable(publish_state):
        unconfirmed_states = (
            *unconfirmed_states[:i],
            replace(publish_state, routing_state=ReturnedRoutingState()),
            *unconfirmed_states[i + 1 :],
        )
    else:
        unconfirmed_states = (
            *unconfirmed_states[:i],
            *unconfirmed_states[i + 1 :],
        )

    channel_state = replace(channel_state, unconfirmed_states=unconfirmed_states)
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state


def retire_unconfirmed(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    confirm_seq: int,
    multiple: bool,
) -> ModelState:
    def is_confirmed(publish_state: PublishState) -> bool:
        return is_confirmed_by(publish_state, confirm_seq, multiple)

    n = max((i + 1 for i in publish_indices(state.line, is_confirmed)), default=0)
    state, channel_state = advance_line(state, connection_state, channel_state, n)

    unconfirmed_states = tuple(
        publish_state
        for publish_state in channel_state.unconfirmed_states
        if not is_confirmed(publish_state)
    )
    channel_state = replace(channel_state, unconfirmed_states=unconfirmed_states)
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state


def filter_deliveries(
    channel_state: OpenChannelState, delivery_tag: int, multiple: bool
) -> tuple[DeliveryState, ...]:
    if multiple and delivery_tag == 0:  # acknowledge-all
        return channel_state.unacked_states

    if multiple:
        return tuple(
            delivery
            for delivery in channel_state.unacked_states
            if delivery.delivery_tag <= delivery_tag
        )
    else:
        return tuple(
            delivery
            for delivery in channel_state.unacked_states
            if delivery.delivery_tag == delivery_tag
        )


def retire_deliveries(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    deliveries: tuple[DeliveryState, ...],
) -> ModelState:
    assert all(delivery in channel_state.unacked_states for delivery in deliveries)

    unacked_states = tuple(
        unacked_state
        for unacked_state in channel_state.unacked_states
        if unacked_state not in deliveries
    )
    channel_state = replace(channel_state, unacked_states=unacked_states)
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state


def requeue_deliveries(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    deliveries: tuple[DeliveryState, ...],
) -> ModelState:
    requeued_message_states = state.requeued_message_states + tuple(
        delivery.message_state for delivery in deliveries
    )
    state = replace(state, requeued_message_states=requeued_message_states)
    state = retire_deliveries(state, connection_state, channel_state, deliveries)
    return state


###   ==========================
###    Simple state transitions
###   ==========================


def to_confirming_state(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
) -> ModelState:
    match channel_state.confirm_state:
        case ConfirmingState():
            return state

        case NotConfirmingState():
            channel_state = replace(channel_state, confirm_state=ConfirmingState())
            connection_state = replace(connection_state, channel_state=channel_state)
            state = replace(state, connection_state=connection_state)
            return state


# N.b. unused parameter at runtime gives nice type-checking and documentation
def to_no_consumer_state(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    consumer_state: ActiveConsumerState | CancelledConsumerState,
) -> ModelState:
    channel_state = replace(channel_state, consumer_state=NoConsumerState())
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state


def to_cancelled_consumer_state(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    consumer_state: ActiveConsumerState,
) -> ModelState:
    channel_state = replace(
        channel_state,
        consumer_state=CancelledConsumerState(consumer_state.consumer_tag),
    )
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state


def to_active_consumer_state(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
    consumer_state: CancelledConsumerState | NoConsumerState,
    consumer_tag: str,
    no_ack: bool,
) -> ModelState:
    channel_state = replace(
        channel_state,
        consumer_state=ActiveConsumerState(consumer_tag=consumer_tag, no_ack=no_ack),
    )
    connection_state = replace(connection_state, channel_state=channel_state)
    state = replace(state, connection_state=connection_state)
    return state


def to_open_channel_state(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: ClosedChannelState,
) -> ModelState:
    connection_state = replace(connection_state, channel_state=OpenChannelState())
    state = replace(state, connection_state=connection_state)
    return state


def to_closing_channel_state(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: OpenChannelState,
) -> ModelState:
    state = owe_no_replies(state)
    state = requeue_deliveries(
        state, connection_state, channel_state, channel_state.unacked_states
    )
    connection_state = replace(connection_state, channel_state=ClosingChannelState())
    state = replace(state, connection_state=connection_state)
    return state


def to_closed_channel_state(
    state: ModelState,
    connection_state: OpenConnectionState,
    channel_state: ClosingChannelState,
) -> ModelState:
    state = owe_no_replies(state)
    connection_state = replace(connection_state, channel_state=ClosedChannelState())
    state = replace(state, connection_state=connection_state)
    return state


def to_closing_connection_state(
    state: ModelState, connection_state: TunedConnectionState | OpenConnectionState
) -> ModelState:
    state = owe_no_replies(state)

    match connection_state:
        case OpenConnectionState(channel_state=OpenChannelState() as channel_state):
            state = requeue_deliveries(
                state, connection_state, channel_state, channel_state.unacked_states
            )
        case _:
            pass

    state = replace(state, connection_state=ClosingConnectionState())
    return state


def to_closed_connection_state(
    state: ModelState, connection_state: ClosingConnectionState
) -> ModelState:
    state = owe_no_replies(state)
    state = replace(state, connection_state=ClosedConnectionState())
    return state


def to_greeted_connection_state(
    state: ModelState, connection_state: ClosedConnectionState
) -> ModelState:
    state = owe_no_replies(state)
    state = replace(state, connection_state=GreetedConnectionState())
    return state


def to_started_connection_state(
    state: ModelState, connection_state: GreetedConnectionState
) -> ModelState:
    state = owe_no_replies(state)
    state = replace(state, connection_state=StartedConnectionState())
    return state


def to_tuned_connection_state(
    state: ModelState, connection_state: StartedConnectionState
) -> ModelState:
    state = owe_no_replies(state)
    state = replace(state, connection_state=TunedConnectionState())
    return state


def to_open_connection_state(
    state: ModelState, connection_state: TunedConnectionState
) -> ModelState:
    state = owe_no_replies(state)
    state = replace(state, connection_state=OpenConnectionState())
    return state


type TransitionOutcome = ModelState | Unrepresentable
