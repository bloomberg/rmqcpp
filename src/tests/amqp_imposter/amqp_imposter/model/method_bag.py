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


def sendables(state: ms.ModelState) -> tuple[m.Method, ...]:
    match connection_state := state.connection_state:
        case ms.ClosedConnectionState():
            return ()
        case ms.OpenConnectionState():
            return sendable_ordered_methods(state) + sendable_standby_methods(
                state, connection_state
            )
        case _:
            return sendable_ordered_methods(state)


def sendable_ordered_methods(state: ms.ModelState) -> tuple[m.Method, ...]:
    return tuple(
        method
        for _, reply in ms.observable_replies(state)
        for method in expected_methods(state, reply)
    )


def expected_methods(
    state: ms.ModelState, reply: ms.ReplyState
) -> tuple[m.Method, ...]:
    match reply:
        case ms.KnownReplyState(causing_method=m.BasicConsume(no_wait=True) as consume):
            reply_methods = reply.expected_methods + expected_consumer_methods(
                state, consume
            )
        case ms.KnownReplyState():
            reply_methods = reply.expected_methods
        case ms.DeferredReplyState(causing_method=m.BasicGet()):
            reply_methods = expected_get_methods(state)

    return reply_methods + er.optional_error_replies(reply.causing_method)


def expected_consumer_methods(
    state: ms.ModelState, consume: m.BasicConsume
) -> tuple[m.Method, ...]:
    match state.connection_state:
        case ms.OpenConnectionState(
            channel_state=ms.OpenChannelState(
                consumer_state=ms.ActiveConsumerState(consumer_tag=consumer_tag)
            ) as channel_state
        ) if consumer_tag == consume.consumer_tag:
            return sendable_consumer_methods(state, channel_state)
        case _:
            return ()


def expected_get_methods(state: ms.ModelState) -> tuple[m.Method, ...]:
    match state.connection_state:
        case ms.OpenConnectionState(
            channel_state=ms.OpenChannelState() as channel_state
        ):
            return (*sendable_get_oks(state, channel_state), m.BasicGetEmpty())
        case _:
            return ()


def sendable_get_oks(
    state: ms.ModelState, channel_state: ms.OpenChannelState
) -> tuple[m.BasicGetOk, ...]:
    delivery_tag = channel_state.next_delivery_tag

    fresh_get_ok = m.BasicGetOk(delivery_tag=delivery_tag)
    requeued_get_oks = tuple(
        m.BasicGetOk(
            delivery_tag=delivery_tag,
            redelivered=True,
            exchange=message.exchange,
            routing_key=message.routing_key,
            properties=message.properties,
            body=message.body,
        )
        for message in state.requeued_message_states
    )
    return (fresh_get_ok, *requeued_get_oks)


def sendable_standby_methods(
    state: ms.ModelState, connection_state: ms.OpenConnectionState
) -> tuple[m.Method, ...]:
    match channel_state := connection_state.channel_state:
        case ms.OpenChannelState():
            return sendable_unprompted_closes() + sendable_standby_messages(
                state, channel_state
            )
        case _:
            return sendable_unprompted_closes()


def sendable_unprompted_closes() -> tuple[m.Method, ...]:
    return tuple(
        m.ConnectionClose(
            reply_code=reply_code,
            reply_text=mf.NOT_FORCED_REPLY_TEXT,
            class_id=mf.UNPROMPTED_CLASS_ID,
            method_id=mf.UNPROMPTED_METHOD_ID,
        )
        for reply_code in sorted(mf.UNPROMPTED_ERRORS)
    )


def sendable_standby_messages(
    state: ms.ModelState, channel_state: ms.OpenChannelState
) -> tuple[m.Method, ...]:
    standby_publishes = channel_state.unconfirmed_states + ms.observable_publishes(
        state
    )
    return (
        sendable_standby_consumer_methods(state, channel_state)
        + sendable_returns_for(standby_publishes)
        + sendable_confirms_for(standby_publishes)
    )


def sendable_standby_consumer_methods(
    state: ms.ModelState, channel_state: ms.OpenChannelState
) -> tuple[m.Method, ...]:
    if ms.has_pending_consume(state):
        return ()

    return sendable_consumer_methods(state, channel_state)


def sendable_consumer_methods(
    state: ms.ModelState, channel_state: ms.OpenChannelState
) -> tuple[m.Method, ...]:
    match consumer_state := channel_state.consumer_state:
        case ms.ActiveConsumerState():
            return sendable_consumer_cancels(
                state, consumer_state
            ) + sendable_consumer_deliveries(state, channel_state, consumer_state)
        case ms.CancelledConsumerState() | ms.NoConsumerState():
            return ()


def sendable_consumer_cancels(
    state: ms.ModelState, consumer_state: ms.ActiveConsumerState
) -> tuple[m.Method, ...]:
    if not state.negotiation_options.consumer_cancel_notify:
        return ()

    return (m.BasicCancel(consumer_tag=consumer_state.consumer_tag, no_wait=True),)


def sendable_consumer_deliveries(
    state: ms.ModelState,
    channel_state: ms.OpenChannelState,
    consumer_state: ms.ActiveConsumerState,
) -> tuple[m.Method, ...]:
    if not ms.has_prefetch_space(channel_state, consumer_state):
        return ()

    delivery_tag = channel_state.next_delivery_tag

    # a fresh delivery with arbitrary fields
    fresh_delivery = m.BasicDeliver(
        consumer_tag=consumer_state.consumer_tag,
        delivery_tag=delivery_tag,
    )
    requeued_deliveries = tuple(
        m.BasicDeliver(
            consumer_tag=consumer_state.consumer_tag,
            delivery_tag=delivery_tag,
            redelivered=True,
            exchange=message.exchange,
            routing_key=message.routing_key,
            properties=message.properties,
            body=message.body,
        )
        for message in state.requeued_message_states
    )
    return (fresh_delivery, *requeued_deliveries)


def sendable_returns_for(
    publish_states: tuple[ms.PublishState, ...],
) -> tuple[m.Method, ...]:
    return tuple(
        m.BasicReturn(
            reply_code=mf.ReplyCode.UNROUTABLE,
            reply_text=mf.GEN_NONEMPTY_SHORTSTR,
            exchange=publish_state.message_state.exchange,
            routing_key=publish_state.message_state.routing_key,
            properties=publish_state.message_state.properties,
            body=publish_state.message_state.body,
        )
        for publish_state in publish_states
        if ms.is_returnable(publish_state)
    )


def sendable_confirms_for(
    publishes: tuple[ms.PublishState, ...],
) -> tuple[m.Method, ...]:
    return sendable_ack_confirms_for(publishes) + sendable_nack_confirms_for(publishes)


def sendable_ack_confirms_for(
    publishes: tuple[ms.PublishState, ...],
) -> tuple[m.Method, ...]:
    # we can ack any unconfirmed publish regardless of whether it has been returned
    return tuple(
        m.BasicAck(
            delivery_tag=confirm_seq,  # confirms reuse the delivery-tag field
            multiple=multiple,
        )
        for multiple in (False, True)
        for confirm_seq in ms.confirm_seqs(publishes)
    )


def sendable_nack_confirms_for(
    publishes: tuple[ms.PublishState, ...],
) -> tuple[m.Method, ...]:
    returned = ms.returned_confirm_seqs(publishes)
    min_returned = min(returned, default=None)

    # we can single-nack any unconfirmed publish that has not been returned
    # and multiple-nack any unconfirmed publish below min_returned
    return tuple(
        m.BasicNack(
            delivery_tag=confirm_seq,
            multiple=multiple,
            requeue=mf.GEN_BIT,  # the requeue field is arbitrary when the server nacks
        )
        for multiple in (False, True)
        for confirm_seq in ms.confirm_seqs(publishes)
        if confirm_seq not in returned
        and (not multiple or min_returned is None or confirm_seq < min_returned)
    )
