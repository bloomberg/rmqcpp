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

from . import error_replies as er
from . import method_fields as mf
from . import methods as m
from . import model_state as ms
from .constants import (
    DELIVERY_MODES,
    PRIORITY_MAX,
    PRIORITY_MIN,
    PROTOCOL_MAJOR,
    PROTOCOL_MINOR,
)
from .method_fields import assert_concrete


def apply(state: ms.ModelState, method: m.Method) -> ms.TransitionOutcome:
    match connection_state := state.connection_state:
        case ms.ClosedConnectionState():
            return ms.Unrepresentable(method)
        case ms.GreetedConnectionState():
            return apply_greeted(state, connection_state, method)
        case ms.StartedConnectionState():
            return apply_started(state, connection_state, method)
        case ms.TunedConnectionState():
            return apply_tuned(state, connection_state, method)
        case ms.OpenConnectionState():
            return apply_open(state, connection_state, method)
        case ms.ClosingConnectionState():
            return apply_closing(state, connection_state, method)


def apply_protocol_header(state: ms.ModelState) -> ms.TransitionOutcome:
    match state.connection_state:
        case ms.ClosedConnectionState():
            state = ms.to_greeted_connection_state(state, state.connection_state)
            state = ms.owe_reply(
                state,
                m.ConnectionStart(
                    version_major=PROTOCOL_MAJOR,
                    version_minor=PROTOCOL_MINOR,
                    mechanisms=state.negotiation_options.mechanisms,
                    locales=state.negotiation_options.locales,
                    publisher_confirms=state.negotiation_options.publisher_confirms,
                    basic_nack=state.negotiation_options.basic_nack,
                    consumer_cancel_notify=state.negotiation_options.consumer_cancel_notify,
                ),
            )
            return state
        case _:
            return ms.Unrepresentable()


def owe_error(
    state: ms.ModelState, reply_code: mf.ReplyCode, causing_method: m.Method
) -> ms.ModelState:
    return ms.owe_reply(
        state, er.error_reply(reply_code, mf.FORCED_REPLY_TEXT, causing_method)
    )


def method_queue_declare_ok(method: m.QueueDeclare) -> m.QueueDeclareOk:
    queue = assert_concrete(method.queue)

    if queue == "":
        # anonymous queue; owe a reply with arbitrary queue name
        return m.QueueDeclareOk()
    else:
        return m.QueueDeclareOk(queue=queue)


def method_basic_consume_ok(consumer_tag: str) -> m.BasicConsumeOk:
    if consumer_tag == "":
        # anonymous consumer; owe a reply with arbitrary consumer tag
        return m.BasicConsumeOk()
    else:
        return m.BasicConsumeOk(consumer_tag=consumer_tag)


def apply_greeted(
    state: ms.ModelState, connection_state: ms.GreetedConnectionState, method: m.Method
) -> ms.TransitionOutcome:
    options = state.negotiation_options

    match method:
        case m.ConnectionStartOk() if (
            assert_concrete(method.mechanism) in options.mechanisms
            and assert_concrete(method.locale) in options.locales
        ):
            state = ms.to_started_connection_state(state, connection_state)
            state = ms.owe_reply(
                state,
                m.ConnectionTune(
                    channel_max=options.channel_max,
                    frame_max=options.frame_max,
                    heartbeat=options.heartbeat,
                ),
            )
            return state
        case _:
            return ms.Unrepresentable(method)


def apply_started(
    state: ms.ModelState,
    connection_state: ms.StartedConnectionState,
    method: m.Method,
) -> ms.TransitionOutcome:
    options = state.negotiation_options

    match method:
        case m.ConnectionTuneOk() if (
            method.channel_max,
            method.frame_max,
            method.heartbeat,
        ) == (options.channel_max, options.frame_max, options.heartbeat):
            state = ms.to_tuned_connection_state(state, connection_state)
            return state
        case _:
            return ms.Unrepresentable(method)


def apply_tuned(
    state: ms.ModelState,
    connection_state: ms.TunedConnectionState,
    method: m.Method,
) -> ms.TransitionOutcome:
    match method:
        case m.ConnectionOpen():
            # not an open connection just yet; cannot be pipelined with e.g. Channel.Open
            state = ms.owe_reply(state, m.ConnectionOpenOk(), method)
            return state
        case _:
            return ms.Unrepresentable(method)


def apply_closing(
    state: ms.ModelState,
    connection_state: ms.ClosingConnectionState,
    method: m.Method,
) -> ms.TransitionOutcome:
    match method:
        case m.ConnectionClose():
            state = ms.owe_no_replies(state)
            state = ms.owe_reply(state, m.ConnectionCloseOk(), method)
            return state
        case m.ConnectionCloseOk():
            return ms.to_closed_connection_state(state, connection_state)
        case _:
            return state


def apply_open(
    state: ms.ModelState, connection_state: ms.OpenConnectionState, method: m.Method
) -> ms.TransitionOutcome:
    match method:
        case (
            m.ConnectionStart()
            | m.ConnectionStartOk()
            | m.ConnectionTune()
            | m.ConnectionTuneOk()
            | m.ConnectionOpen()
            | m.ConnectionOpenOk()
            | m.ConnectionCloseOk()
        ):
            return ms.Unrepresentable(method)
        case m.ConnectionClose():
            state = ms.to_closing_connection_state(state, connection_state)
            state = ms.owe_reply(state, m.ConnectionCloseOk(), method)
            return state
        case _:
            return apply_channel(state, connection_state, method)


def apply_channel(
    state: ms.ModelState, connection_state: ms.OpenConnectionState, method: m.Method
) -> ms.TransitionOutcome:
    match channel_state := connection_state.channel_state:
        case ms.OpenChannelState():
            return apply_open_channel(state, connection_state, channel_state, method)
        case ms.ClosingChannelState():
            return apply_closing_channel(state, connection_state, channel_state, method)
        case ms.ClosedChannelState():
            return apply_closed_channel(state, connection_state, channel_state, method)


def apply_closed_channel(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.ClosedChannelState,
    method: m.Method,
) -> ms.TransitionOutcome:
    match method:
        case m.ChannelOpen():
            state = ms.to_open_channel_state(state, connection_state, channel_state)
            state = ms.owe_reply(state, m.ChannelOpenOk(), method)
            return state
        case m.ChannelCloseOk():
            return ms.Unrepresentable(method)
        case _:
            state = owe_error(state, mf.ReplyCode.CHANNEL_ERROR, method)
            return state


def apply_closing_channel(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.ClosingChannelState,
    method: m.Method,
) -> ms.TransitionOutcome:
    match method:
        case m.ChannelClose():
            state = ms.owe_reply(state, m.ChannelCloseOk())
            return state
        case m.ChannelCloseOk():
            state = ms.to_closed_channel_state(state, connection_state, channel_state)
            return state
        case _:
            return state


def apply_open_channel(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.Method,
) -> ms.TransitionOutcome:
    if isinstance(method, m.ChannelCloseOk):
        return ms.Unrepresentable(method)

    if ms.has_terminal_error(state):
        return state

    match method:
        case m.ChannelOpen():
            state = owe_error(state, mf.ReplyCode.CHANNEL_ERROR, method)
            return state
        case m.ChannelClose():
            state = ms.to_closing_channel_state(state, connection_state, channel_state)
            state = ms.owe_reply(state, m.ChannelCloseOk())
            return state
        case m.ConfirmSelect():
            return apply_confirm_select(state, connection_state, channel_state, method)
        case m.ExchangeDeclare():
            return apply_unless_no_wait(state, method, m.ExchangeDeclareOk())
        case m.ExchangeBind():
            return apply_unless_no_wait(state, method, m.ExchangeBindOk())
        case m.QueueDeclare():
            return apply_unless_no_wait(state, method, method_queue_declare_ok(method))
        case m.QueueBind():
            return apply_unless_no_wait(state, method, m.QueueBindOk())
        case m.BasicQos():
            return apply_basic_qos(state, method)
        case m.BasicConsume():
            return apply_basic_consume(state, connection_state, channel_state, method)
        case m.BasicCancel():
            return apply_basic_cancel(state, connection_state, channel_state, method)
        case m.BasicGet():
            return ms.owe_get_reply(state, method)
        case m.BasicPublish():
            return apply_basic_publish(state, connection_state, channel_state, method)
        case m.BasicAck():
            return apply_ack(state, connection_state, channel_state, method)
        case m.BasicNack():
            return apply_nack(state, connection_state, channel_state, method)
        case m.BasicReject():
            return apply_reject(state, connection_state, channel_state, method)
        case _:
            return ms.Unrepresentable(method)


def apply_unless_no_wait(
    state: ms.ModelState,
    causing_method: m.ExchangeDeclare | m.ExchangeBind | m.QueueDeclare | m.QueueBind,
    reply_method: m.Method,
) -> ms.TransitionOutcome:
    # a received method must define no-wait
    # we should not be here if it doesn't
    no_wait = assert_concrete(causing_method.no_wait)

    if no_wait:
        state = ms.owe_optional_reply(state, causing_method)
        return state
    else:
        state = ms.owe_reply(state, reply_method, causing_method)
        return state


def apply_confirm_select(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.ConfirmSelect,
) -> ms.TransitionOutcome:
    no_wait = assert_concrete(method.no_wait)

    state = ms.to_confirming_state(state, connection_state, channel_state)

    if no_wait:
        return state
    else:
        state = ms.owe_reply(state, m.ConfirmSelectOk(), method)
        return state


def apply_basic_qos(state: ms.ModelState, method: m.BasicQos) -> ms.TransitionOutcome:
    global_qos = assert_concrete(method.global_qos)
    prefetch_size = assert_concrete(method.prefetch_size)

    if global_qos:
        return ms.Unrepresentable(method)

    if prefetch_size != 0:
        state = owe_error(state, mf.ReplyCode.NOT_IMPLEMENTED, method)
        return state
    else:
        state = ms.owe_reply(state, m.BasicQosOk(), method)
        return state


def apply_basic_consume(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicConsume,
) -> ms.TransitionOutcome:
    consumer_tag = assert_concrete(method.consumer_tag)
    no_wait = assert_concrete(method.no_wait)

    match consumer_state := channel_state.consumer_state:
        case ms.ActiveConsumerState():
            return ms.Unrepresentable(method)

        case ms.CancelledConsumerState() | ms.NoConsumerState():
            if ms.awaits_consume_ok(state):
                return ms.Unrepresentable(method)

            match no_wait, consumer_tag:
                case True, "":
                    return ms.Unrepresentable(method)
                case True, _:
                    no_ack = assert_concrete(method.no_ack)
                    state = ms.to_active_consumer_state(
                        state,
                        connection_state,
                        channel_state,
                        consumer_state,
                        consumer_tag,
                        no_ack,
                    )
                    return ms.owe_optional_reply(state, method)
                case False, _:
                    state = ms.owe_reply(
                        state, method_basic_consume_ok(consumer_tag), method
                    )
                    return state


def apply_basic_cancel(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicCancel,
) -> ms.TransitionOutcome:
    no_wait = assert_concrete(method.no_wait)
    consumer_tag = assert_concrete(method.consumer_tag)

    match consumer_state := channel_state.consumer_state:
        case ms.ActiveConsumerState() | ms.CancelledConsumerState():
            # possibly reconcile our cancel with the client's
            if consumer_state.consumer_tag != consumer_tag:
                return ms.Unrepresentable(method)

            if no_wait:
                return ms.to_no_consumer_state(
                    state, connection_state, channel_state, consumer_state
                )
            else:
                return ms.owe_reply(
                    state, m.BasicCancelOk(consumer_tag=consumer_tag), method
                )
        case ms.NoConsumerState():
            return ms.Unrepresentable(method)


def apply_basic_publish(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicPublish,
) -> ms.TransitionOutcome:
    immediate = assert_concrete(method.immediate)
    properties = assert_concrete(method.properties)
    mandatory = assert_concrete(method.mandatory)
    routing_key = assert_concrete(method.routing_key)
    exchange = assert_concrete(method.exchange)
    body = assert_concrete(method.body)

    if immediate:
        state = owe_error(state, mf.ReplyCode.NOT_IMPLEMENTED, method)
        return state

    if not validate_properties(properties):
        # TODO should we send PRECONDITION_FAILED instead
        return ms.Unrepresentable(method)

    state = ms.owe_optional_reply(state, method)

    # if not mandatory, we know straight away it won't be returned
    routing_state = (
        ms.ReturnableRoutingState() if mandatory else ms.UnreturnableRoutingState()
    )
    confirm_seq = ms.maybe_make_confirm_seq(channel_state)

    state = ms.owe_publish(
        state,
        connection_state,
        channel_state,
        ms.PublishState(
            message_state=ms.MessageState(
                exchange=exchange,
                routing_key=routing_key,
                properties=properties,
                body=body,
            ),
            confirm_seq=confirm_seq,
            routing_state=routing_state,
        ),
    )
    return state


def validate_priority(priority: int | None) -> bool:
    return priority is None or PRIORITY_MIN <= priority <= PRIORITY_MAX


def validate_delivery_mode(delivery_mode: int | None) -> bool:
    return delivery_mode is None or delivery_mode in DELIVERY_MODES


def validate_properties(properties: mf.MessageProperties | None) -> bool:
    if properties is None:
        return True

    return validate_priority(properties.priority) and validate_delivery_mode(
        properties.delivery_mode
    )


def apply_ack(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicAck,
) -> ms.TransitionOutcome:
    multiple = assert_concrete(method.multiple)
    delivery_tag = assert_concrete(method.delivery_tag)

    if not ms.is_acknowledgeable(channel_state, delivery_tag, multiple):
        state = owe_error(state, mf.ReplyCode.PRECONDITION_FAILED, method)
        return state

    state = ms.retire_deliveries(
        state,
        connection_state,
        channel_state,
        ms.filter_deliveries(channel_state, delivery_tag, multiple),
    )
    return state


def apply_nack(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicNack,
) -> ms.TransitionOutcome:
    multiple = assert_concrete(method.multiple)
    requeue = assert_concrete(method.requeue)
    delivery_tag = assert_concrete(method.delivery_tag)

    if not ms.is_acknowledgeable(channel_state, delivery_tag, multiple):
        state = owe_error(state, mf.ReplyCode.PRECONDITION_FAILED, method)
        return state

    filtered = ms.filter_deliveries(channel_state, delivery_tag, multiple)

    if requeue:
        state = ms.requeue_deliveries(state, connection_state, channel_state, filtered)
        return state
    else:
        state = ms.retire_deliveries(state, connection_state, channel_state, filtered)
        return state


def apply_reject(
    state: ms.ModelState,
    connection_state: ms.OpenConnectionState,
    channel_state: ms.OpenChannelState,
    method: m.BasicReject,
) -> ms.TransitionOutcome:
    multiple = False
    requeue = assert_concrete(method.requeue)
    delivery_tag = assert_concrete(method.delivery_tag)

    if not ms.is_acknowledgeable(channel_state, delivery_tag, multiple):
        state = owe_error(state, mf.ReplyCode.PRECONDITION_FAILED, method)
        return state

    filtered = ms.filter_deliveries(channel_state, delivery_tag, multiple)

    if requeue:
        state = ms.requeue_deliveries(state, connection_state, channel_state, filtered)
        return state
    else:
        state = ms.retire_deliveries(state, connection_state, channel_state, filtered)
        return state
