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

from collections.abc import Callable
from dataclasses import replace

from amqp_imposter.model import (
    DEFAULT_NEGOTIATION_OPTIONS,
    Arbitrary,
    Exact,
    Method,
    ModelServer,
    NegotiationOptions,
    Unrepresentable,
    assert_concrete,
)
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

EXCHANGE = "ex"
ROUTING_KEY = "rk"
QUEUE = "q"
OTHER_QUEUE = "q-other"
CONSUMER_TAG = "ctag-1"
OTHER_CONSUMER_TAG = "ctag-2"

MESSAGE_HEADERS = mf.Table({"x-origin": "imposter", "x-retry": 1})
QUEUE_ARGUMENTS = mf.Table({"x-max-length": 10, "x-queue-type": "quorum"})
MESSAGE_PROPERTIES = mf.MessageProperties(
    content_type="text/plain", message_id="m-1", headers=MESSAGE_HEADERS
)

GRACEFUL_CHANNEL_CLOSE = m.ChannelClose(
    mf.ReplyCode.SUCCESS,
    "Normal shutdown",
    mf.UNPROMPTED_CLASS_ID,
    mf.UNPROMPTED_METHOD_ID,
)
GRACEFUL_CONNECTION_CLOSE = m.ConnectionClose(
    mf.ReplyCode.SUCCESS,
    "Normal shutdown",
    mf.UNPROMPTED_CLASS_ID,
    mf.UNPROMPTED_METHOD_ID,
)
FRESH_DELIVERY = m.BasicDeliver(consumer_tag=CONSUMER_TAG, delivery_tag=1)

ACKNOWLEDGEMENT_TYPES = (m.BasicAck, m.BasicNack, m.BasicReject)

ErrorReply = tuple[type[m.ChannelClose | m.ConnectionClose], int]


def lookup_sendables[M: Method](
    server: ModelServer, kind: type[M], **fields: object
) -> tuple[M, ...]:
    return tuple(
        method
        for method in server.sendables()
        if isinstance(method, kind)
        and all(getattr(method, name) == value for name, value in fields.items())
    )


def lookup_unique_sendable[M: Method](
    server: ModelServer, kind: type[M], **fields: object
) -> M:
    found = lookup_sendables(server, kind, **fields)
    assert len(found) == 1, found
    return found[0]


def subsets_any_sendable(
    server: ModelServer, kind: type[Method], **fields: object
) -> bool:
    return lookup_sendables(server, kind, **fields) != ()


def error_replies_to(server: ModelServer, kind: type[Method]) -> set[ErrorReply]:
    return {
        (type(method), assert_concrete(method.reply_code))
        for method in server.sendables()
        if isinstance(method, (m.ChannelClose, m.ConnectionClose))
        and (method.class_id, method.method_id) == (kind.CLASS_ID, kind.METHOD_ID)
    }


def channel_errors(*reply_codes: mf.ReplyCode) -> set[ErrorReply]:
    return {(m.ChannelClose, reply_code) for reply_code in reply_codes}


def connection_errors(*reply_codes: mf.ReplyCode) -> set[ErrorReply]:
    return {(m.ConnectionClose, reply_code) for reply_code in reply_codes}


def match_fresh_delivery(server: ModelServer) -> m.BasicDeliver:
    # the fresh delivery is the only one where its redelivery status is arbitrary!
    return lookup_unique_sendable(server, m.BasicDeliver, redelivered=mf.GEN_BIT)


def match_fresh_get_ok(server: ModelServer) -> m.BasicGetOk:
    return lookup_unique_sendable(server, m.BasicGetOk, redelivered=mf.GEN_BIT)


def send_unwrap[M: Method](server: ModelServer, kind: type[M], **fields: object) -> M:
    method = lookup_unique_sendable(server, kind, **fields)
    sent = server.send(method)
    assert isinstance(sent, kind), sent
    return sent


def receive_unwrap(server: ModelServer, method: Method) -> Method:
    received = server.receive(method)
    assert not isinstance(received, Unrepresentable), (method, received)
    return received


def delivery_tags(
    server: ModelServer, kind: type[m.BasicAck | m.BasicNack]
) -> set[tuple[Exact[int], Arbitrary[bool]]]:
    # get the (delivery_tag, multiple) tuples for all sendable BasicAcks and BasicNacks
    # N.b. there is no BasicReject as the server doesn't send that
    return {
        (method.delivery_tag, method.multiple)
        for method in server.sendables()
        if isinstance(method, kind)
    }


def message_content(
    message: m.BasicDeliver | m.BasicGetOk | m.BasicPublish | m.BasicReturn,
) -> tuple[
    Arbitrary[str] | Exact[str],
    Arbitrary[str] | Exact[str],
    Arbitrary[mf.MessageProperties | None],
    Arbitrary[bytes],
]:
    return (message.exchange, message.routing_key, message.properties, message.body)


def greeted(options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS) -> ModelServer:
    server = ModelServer(options)
    assert server.receive_protocol_header() is None
    return server


def start_ok_of(start: m.ConnectionStart) -> m.ConnectionStartOk:
    mechanisms = assert_concrete(start.mechanisms)
    locales = assert_concrete(start.locales)
    return m.ConnectionStartOk(mechanism=mechanisms[0], locale=locales[0])


def tune_ok_of(tune: m.ConnectionTune) -> m.ConnectionTuneOk:
    return m.ConnectionTuneOk(tune.channel_max, tune.frame_max, tune.heartbeat)


def starting(options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS) -> ModelServer:
    server = greeted(options)
    send_unwrap(server, m.ConnectionStart)
    return server


def tuning(options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS) -> ModelServer:
    server = greeted(options)
    receive_unwrap(server, start_ok_of(send_unwrap(server, m.ConnectionStart)))
    return server


def tuned(options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS) -> ModelServer:
    server = tuning(options)
    receive_unwrap(server, tune_ok_of(send_unwrap(server, m.ConnectionTune)))
    return server


def opening(options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS) -> ModelServer:
    server = tuned(options)
    receive_unwrap(server, m.ConnectionOpen())
    return server


def connected(options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS) -> ModelServer:
    server = opening(options)
    send_unwrap(server, m.ConnectionOpenOk)
    return server


def channelled(
    options: NegotiationOptions = DEFAULT_NEGOTIATION_OPTIONS,
) -> ModelServer:
    server = connected(options)
    receive_unwrap(server, m.ChannelOpen())
    send_unwrap(server, m.ChannelOpenOk)
    return server


def confirming() -> ModelServer:
    server = channelled()
    receive_unwrap(server, m.ConfirmSelect())
    send_unwrap(server, m.ConfirmSelectOk)
    return server


def consume(server: ModelServer, no_ack: bool = False) -> None:
    receive_unwrap(
        server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG, no_ack=no_ack)
    )
    send_unwrap(server, m.BasicConsumeOk)


def consuming(prefetch_count: int = 1, no_ack: bool = False) -> ModelServer:
    server = channelled()
    receive_unwrap(server, m.BasicQos(prefetch_count=prefetch_count))
    send_unwrap(server, m.BasicQosOk)
    consume(server, no_ack=no_ack)
    return server


def consuming_zero_prefetch_count() -> ModelServer:
    return consuming(prefetch_count=0)


def consuming_no_ack() -> ModelServer:
    return consuming(no_ack=True)


def consuming_no_qos() -> ModelServer:
    server = channelled()
    consume(server)
    return server


def deliver(server: ModelServer, delivery_tag: int) -> m.BasicDeliver:
    return send_unwrap(
        server, m.BasicDeliver, delivery_tag=delivery_tag, redelivered=mf.GEN_BIT
    )


def prefetch_count_filled(prefetch_count: int) -> ModelServer:
    server = consuming(prefetch_count=prefetch_count)
    for delivery_tag in range(1, prefetch_count + 1):
        deliver(server, delivery_tag)
    return server


def publish(
    server: ModelServer, body: bytes = b"m", mandatory: bool = False
) -> m.BasicPublish:
    published = m.BasicPublish(
        exchange=EXCHANGE,
        routing_key=ROUTING_KEY,
        mandatory=mandatory,
        properties=MESSAGE_PROPERTIES,
        body=body,
    )
    receive_unwrap(server, published)
    return published


def publishing() -> ModelServer:
    server = confirming()
    publish(server)
    return server


def declaring() -> ModelServer:
    server = channelled()
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    return server


def get(server: ModelServer, no_ack: bool = False) -> None:
    receive_unwrap(server, m.BasicGet(queue=QUEUE, no_ack=no_ack))


def polling() -> ModelServer:
    server = channelled()
    get(server)
    return server


def polled() -> ModelServer:
    server = polling()
    send_unwrap(server, m.BasicGetOk)
    return server


def send_get_ok(server: ModelServer, body: bytes = b"m") -> m.BasicGetOk:
    got = server.send(replace(match_fresh_get_ok(server), body=body))
    assert isinstance(got, m.BasicGetOk), got
    return got


def send_cancel(server: ModelServer) -> None:
    send_unwrap(server, m.BasicCancel, consumer_tag=CONSUMER_TAG)


def receive_cancel(server: ModelServer) -> None:
    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG))
    send_unwrap(server, m.BasicCancelOk, consumer_tag=CONSUMER_TAG)


def enable_channel_error(server: ModelServer) -> None:
    receive_unwrap(server, m.BasicAck(delivery_tag=0))


def send_channel_error(server: ModelServer) -> m.ChannelClose:
    return send_unwrap(
        server,
        m.ChannelClose,
        reply_code=mf.ReplyCode.PRECONDITION_FAILED,
        class_id=m.BasicAck.CLASS_ID,
        method_id=m.BasicAck.METHOD_ID,
    )


def close_channel_from_server(server: ModelServer) -> None:
    enable_channel_error(server)
    send_channel_error(server)
    receive_unwrap(server, m.ChannelCloseOk())


def close_channel_from_client(server: ModelServer) -> None:
    receive_unwrap(server, GRACEFUL_CHANNEL_CLOSE)
    send_unwrap(server, m.ChannelCloseOk)


def close_connection_from_server(server: ModelServer) -> None:
    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.CONNECTION_FORCED)
    receive_unwrap(server, m.ConnectionCloseOk())


def close_connection_from_client(server: ModelServer) -> None:
    receive_unwrap(server, GRACEFUL_CONNECTION_CLOSE)
    send_unwrap(server, m.ConnectionCloseOk)


def closed() -> ModelServer:
    server = connected()
    close_connection_from_client(server)
    return server


def reopen_channel(server: ModelServer) -> None:
    receive_unwrap(server, m.ChannelOpen())
    send_unwrap(server, m.ChannelOpenOk)


def reconnect(server: ModelServer) -> None:
    assert server.receive_protocol_header() is None
    receive_unwrap(server, start_ok_of(send_unwrap(server, m.ConnectionStart)))
    receive_unwrap(server, tune_ok_of(send_unwrap(server, m.ConnectionTune)))
    receive_unwrap(server, m.ConnectionOpen())
    send_unwrap(server, m.ConnectionOpenOk)
    reopen_channel(server)


MakeServer = Callable[[], ModelServer]
DoTeardown = Callable[[ModelServer], None]

DO_CANCEL: tuple[DoTeardown, ...] = (send_cancel, receive_cancel)
DO_CHANNEL_CLOSE: tuple[DoTeardown, ...] = (
    close_channel_from_server,
    close_channel_from_client,
)
DO_CONNECTION_CLOSE: tuple[DoTeardown, ...] = (
    close_connection_from_server,
    close_connection_from_client,
)

CONNECTING_STATES: tuple[MakeServer, ...] = (starting, tuning, tuned, opening)
CONNECTED_STATES: tuple[MakeServer, ...] = (
    connected,
    channelled,
    confirming,
    consuming,
    consuming_zero_prefetch_count,
    consuming_no_qos,
    consuming_no_ack,
    publishing,
    declaring,
    polling,
    polled,
)
GREETED_STATES: tuple[MakeServer, ...] = CONNECTING_STATES + CONNECTED_STATES
