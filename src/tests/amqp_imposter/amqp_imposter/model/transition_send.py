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

from . import method_bag as mb
from . import method_unification as mu
from . import methods as m
from . import model_state as ms
from .method_fields import assert_concrete


def apply(state: ms.ModelState, method: m.Method) -> ms.TransitionOutcome:
    state, causing_method = take_reply(state, method)
    state = apply_connection(state, method, causing_method)
    return state


def take_reply(
    state: ms.ModelState, method: m.Method
) -> tuple[ms.ModelState, m.Method | None]:
    for i, reply in ms.observable_replies(state):
        # check if we can unify with any of the expected methods
        expected_method = mu.unify_any(method, mb.expected_methods(state, reply))

        if expected_method is not None:
            state = ms.retire_reply(state, i)
            return state, reply.causing_method

    return state, None


def apply_connection(
    state: ms.ModelState, method: m.Method, causing_method: m.Method | None
) -> ms.ModelState:
    connection_state = state.connection_state

    match connection_state, method:
        case ms.TunedConnectionState(), m.ConnectionOpenOk():
            state = ms.to_open_connection_state(state, connection_state)
            return state
        case ms.ClosingConnectionState(), m.ConnectionCloseOk():
            state = ms.to_closed_connection_state(state, connection_state)
            return state
        case (
            (ms.TunedConnectionState() | ms.OpenConnectionState()),
            m.ConnectionClose(),
        ):
            state = ms.to_closing_connection_state(state, connection_state)
            return state
        case ms.OpenConnectionState(), _:
            return apply_open(state, connection_state, method, causing_method)
        case _:
            return state


def apply_open(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    method: m.Method,
    causing_method: m.Method | None,
) -> ms.ModelState:
    channel_state = connection_state.channel_state

    match channel_state, method:
        case ms.ClosingChannelState(), m.ChannelCloseOk():
            state = ms.to_closed_channel_state(state, connection_state, channel_state)
            return state
        case ms.OpenChannelState(), _:
            state = apply_open_channel(
                state, connection_state, channel_state, method, causing_method
            )
            return state
        case _:
            return state


def apply_open_channel(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.Method,
    causing_method: m.Method | None,
) -> ms.ModelState:
    match method:
        case m.ChannelClose():
            return ms.to_closing_channel_state(state, connection_state, channel_state)
        case m.BasicQosOk():
            return apply_basic_qos_ok(
                state, connection_state, channel_state, causing_method
            )
        case m.BasicConsumeOk():
            return apply_basic_consume_ok(
                state, connection_state, channel_state, method, causing_method
            )
        case m.BasicCancel():
            return apply_basic_cancel(state, connection_state, channel_state)
        case m.BasicCancelOk():
            return apply_basic_cancel_ok(state, connection_state, channel_state, method)
        case m.BasicDeliver():
            return apply_basic_deliver(state, connection_state, channel_state, method)
        case m.BasicGetOk():
            assert isinstance(causing_method, m.BasicGet), causing_method
            return apply_basic_get_ok(
                state, connection_state, channel_state, method, causing_method
            )
        case m.BasicReturn():
            return apply_basic_return(state, connection_state, channel_state, method)
        case m.BasicAck() | m.BasicNack():
            return apply_acknowledgement(state, connection_state, channel_state, method)
        case _:
            return state


def apply_basic_qos_ok(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    causing_method: m.Method | None,
) -> ms.ModelState:
    match causing_method:
        case m.BasicQos():
            prefetch_count = assert_concrete(causing_method.prefetch_count)
            state = ms.set_prefetch_count(
                state, connection_state, channel_state, prefetch_count
            )
            return state
        case _:
            return state


def apply_basic_consume_ok(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicConsumeOk,
    causing_method: m.Method | None,
) -> ms.ModelState:
    consumer_state = channel_state.consumer_state

    match consumer_state, causing_method:
        case ((ms.CancelledConsumerState() | ms.NoConsumerState()), m.BasicConsume()):
            consumer_tag = assert_concrete(method.consumer_tag)
            no_ack = assert_concrete(causing_method.no_ack)

            state = ms.to_active_consumer_state(
                state,
                connection_state,
                channel_state,
                consumer_state,
                consumer_tag,
                no_ack,
            )
            return state
        case _:
            return state


def apply_basic_cancel(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
) -> ms.ModelState:
    consumer_state = channel_state.consumer_state

    match consumer_state:
        case ms.ActiveConsumerState():
            # the client may not know yet, so remember what we cancelled
            state = ms.to_cancelled_consumer_state(
                state, connection_state, channel_state, consumer_state
            )
            return state
        case ms.CancelledConsumerState() | ms.NoConsumerState():
            return state


def apply_basic_cancel_ok(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicCancelOk,
) -> ms.ModelState:
    consumer_state = channel_state.consumer_state
    consumer_tag = assert_concrete(method.consumer_tag)

    match consumer_state:
        case ms.ActiveConsumerState() | ms.CancelledConsumerState() if (
            consumer_state.consumer_tag == consumer_tag
        ):
            state = ms.to_no_consumer_state(
                state, connection_state, channel_state, consumer_state
            )
            return state
        case ms.ActiveConsumerState() | ms.CancelledConsumerState():
            return state
        case ms.NoConsumerState():
            return state


def apply_basic_deliver(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicDeliver,
) -> ms.ModelState:
    consumer_state = channel_state.consumer_state

    match consumer_state:
        case ms.ActiveConsumerState():
            return apply_delivery(
                state,
                connection_state,
                channel_state,
                method,
                consumer_state.no_ack,
            )
        case ms.CancelledConsumerState() | ms.NoConsumerState():
            return state


def apply_basic_get_ok(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicGetOk,
    causing_method: m.BasicGet,
) -> ms.ModelState:
    return apply_delivery(
        state,
        connection_state,
        channel_state,
        method,
        assert_concrete(causing_method.no_ack),
    )


def apply_delivery(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicDeliver | m.BasicGetOk,
    no_ack: bool,
) -> ms.ModelState:
    redelivered = assert_concrete(method.redelivered)
    delivery_tag = assert_concrete(method.delivery_tag)
    message_state = ms.message_state(method)
    delivery_state = ms.DeliveryState(
        delivery_tag=delivery_tag,
        message_state=message_state,
    )
    if redelivered:
        state = ms.retire_requeued(state, message_state)

    state = ms.maybe_track_delivery(
        state, connection_state, channel_state, no_ack, delivery_state
    )
    return state


def apply_basic_return(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicReturn,
) -> ms.ModelState:
    state = ms.return_unconfirmed(
        state, connection_state, channel_state, ms.message_state(method)
    )
    return state


def apply_acknowledgement(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicAck | m.BasicNack,
) -> ms.ModelState:
    multiple = assert_concrete(method.multiple)
    delivery_tag = assert_concrete(method.delivery_tag)

    state = ms.retire_unconfirmed(
        state, connection_state, channel_state, delivery_tag, multiple
    )
    return state
