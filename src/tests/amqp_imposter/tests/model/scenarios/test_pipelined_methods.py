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

from amqp_imposter.model import Unrepresentable
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    CONSUMER_TAG,
    EXCHANGE,
    QUEUE,
    ROUTING_KEY,
    channel_errors,
    channelled,
    confirming,
    connected,
    connection_errors,
    consuming,
    deliver,
    delivery_tags,
    enable_channel_error,
    error_replies_to,
    lookup_unique_sendable,
    publish,
    receive_unwrap,
    send_unwrap,
    subsets_any_sendable,
    tuned,
)


def test_pipelined_replies_come_in_order() -> None:
    #
    # AFAICT, 0-9-1 only requires reply ordering within each method class.
    # For example, Exhange.Declare -> Queue.Bind can be replied to in the reverse order.
    # Further, while the spec specifies for many methods that they only mutate state once their reply is sent,
    # this is not the case for .Declare.
    # To illustrate (say there is no topology): Queue.BindOk -> Exchange.DeclareOk could be legal
    # in response to Exhange.Declare -> Queue.Bind,
    # since the timing of the state change from the Exchange.Declare is unspecified.
    #
    # We strengthen our model here to require that all replies come in order, regardless of method class.
    # Could weaken this later.
    #
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(
        server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, routing_key=ROUTING_KEY)
    )

    assert not subsets_any_sendable(server, m.QueueBindOk)

    send_unwrap(server, m.QueueDeclareOk)

    assert subsets_any_sendable(server, m.QueueBindOk)


def test_out_of_order_reply_is_illegal() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(
        server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, routing_key=ROUTING_KEY)
    )

    assert isinstance(server.send(m.QueueBindOk()), Unrepresentable)


def test_same_method_replies_come_in_order() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE + "other"))

    assert send_unwrap(server, m.QueueDeclareOk).queue == QUEUE
    assert lookup_unique_sendable(server, m.QueueDeclareOk).queue == QUEUE + "other"


def test_both_replies_to_get_become_sendable() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(server, m.BasicGet(queue=QUEUE))

    assert not subsets_any_sendable(server, m.BasicGetOk)
    assert not subsets_any_sendable(server, m.BasicGetEmpty)

    send_unwrap(server, m.QueueDeclareOk)

    assert subsets_any_sendable(server, m.BasicGetOk)
    assert subsets_any_sendable(server, m.BasicGetEmpty)


def test_pending_reply_does_not_block_asynchronous_sends() -> None:
    server = consuming()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    deliver(server, 1)

    assert subsets_any_sendable(server, m.BasicCancel)
    assert subsets_any_sendable(server, m.ConnectionClose)
    assert subsets_any_sendable(server, m.QueueDeclareOk)


def test_channel_error_comes_in_order() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE))
    enable_channel_error(server)
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE + "later"))

    assert not error_replies_to(server, m.BasicAck)

    send_unwrap(server, m.QueueDeclareOk, queue=QUEUE)

    assert not error_replies_to(server, m.BasicAck)

    send_unwrap(server, m.QueueBindOk)

    assert error_replies_to(server, m.BasicAck) == channel_errors(
        mf.ReplyCode.PRECONDITION_FAILED
    )
    assert not subsets_any_sendable(server, m.QueueDeclareOk)


def test_ok_discards_errors() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))

    assert error_replies_to(server, m.QueueDeclare)

    send_unwrap(server, m.QueueDeclareOk)

    assert not error_replies_to(server, m.QueueDeclare)


def test_error_comes_in_order() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE))

    assert not error_replies_to(server, m.QueueBind)

    send_unwrap(server, m.QueueDeclareOk)

    assert error_replies_to(server, m.QueueBind)


def test_error_discards_pipelined_replies() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, passive=True))
    receive_unwrap(server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE))
    send_unwrap(server, m.ChannelClose, reply_code=mf.ReplyCode.NOT_FOUND)

    assert not subsets_any_sendable(server, m.QueueDeclareOk)
    assert not subsets_any_sendable(server, m.QueueBindOk)
    assert not error_replies_to(server, m.QueueBind)


def test_connection_error_comes_in_order() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE))
    receive_unwrap(server, m.BasicQos(prefetch_size=1))
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE + "later"))

    # other closes are still sendable
    assert not error_replies_to(server, m.BasicQos)

    send_unwrap(server, m.QueueDeclareOk, queue=QUEUE)
    send_unwrap(server, m.QueueBindOk)

    assert error_replies_to(server, m.BasicQos) == connection_errors(
        mf.ReplyCode.NOT_IMPLEMENTED
    )
    assert not subsets_any_sendable(server, m.QueueDeclareOk)


def test_nowait_request_never_pends() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))
    receive_unwrap(
        server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, routing_key=ROUTING_KEY)
    )

    assert subsets_any_sendable(server, m.QueueBindOk)


def test_reply_discards_older_nowait_error() -> None:
    # the error-reply for a nowait method is only an option until
    # another reply is sent; then we've forfeited our chance to send it
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))
    receive_unwrap(server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE))

    assert subsets_any_sendable(server, m.ChannelClose)
    assert subsets_any_sendable(server, m.QueueBindOk)

    send_unwrap(server, m.QueueBindOk)

    assert not subsets_any_sendable(server, m.ChannelClose)


def test_ack_with_older_nowait_error() -> None:
    server = confirming()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))
    publish(server, body=b"one")
    publish(server, body=b"two")

    assert subsets_any_sendable(server, m.ChannelClose)

    send_unwrap(server, m.BasicAck, delivery_tag=2, multiple=False)

    assert not subsets_any_sendable(server, m.ChannelClose)
    assert delivery_tags(server, m.BasicAck) == {(1, False), (1, True)}


def test_return_with_older_nowait_error() -> None:
    server = confirming()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))
    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two", mandatory=True)

    assert subsets_any_sendable(server, m.ChannelClose)

    server.send(lookup_unique_sendable(server, m.BasicReturn, body=b"two"))

    assert not subsets_any_sendable(server, m.ChannelClose)
    assert delivery_tags(server, m.BasicNack) == {(1, False), (1, True)}


def test_return_with_older_reply() -> None:
    server = confirming()

    publish(server, body=b"same", mandatory=True)
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    publish(server, body=b"same", mandatory=True)

    send_unwrap(server, m.BasicReturn)

    assert subsets_any_sendable(server, m.QueueDeclareOk)


def test_send_nowait_error() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))
    send_unwrap(server, m.ChannelClose, reply_code=mf.ReplyCode.PRECONDITION_FAILED)

    # of course there's still ok even if the causing method was nowait
    assert not isinstance(server.receive(m.ChannelCloseOk()), Unrepresentable)


def test_pipelined_declare_after_channel_open() -> None:
    server = connected()

    receive_unwrap(server, m.ChannelOpen())
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))

    assert not subsets_any_sendable(
        server, m.ConnectionClose, reply_code=mf.ReplyCode.CHANNEL_ERROR
    )
    assert not subsets_any_sendable(server, m.QueueDeclareOk)

    send_unwrap(server, m.ChannelOpenOk)

    assert subsets_any_sendable(server, m.QueueDeclareOk, queue=QUEUE)


def test_pipelined_publish_after_channel_open() -> None:
    server = connected()

    receive_unwrap(server, m.ChannelOpen())
    publish(server, mandatory=True)
    send_unwrap(server, m.ChannelOpenOk)

    assert subsets_any_sendable(server, m.BasicReturn)


def test_pipelined_consume_after_channel_open() -> None:
    server = connected()

    receive_unwrap(server, m.ChannelOpen())
    receive_unwrap(server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG))
    send_unwrap(server, m.ChannelOpenOk)
    send_unwrap(server, m.BasicConsumeOk, consumer_tag=CONSUMER_TAG)

    assert subsets_any_sendable(server, m.BasicDeliver, consumer_tag=CONSUMER_TAG)


def test_pipelined_nowait_consume_after_declare() -> None:
    server = channelled()

    receive_unwrap(server, m.QueueDeclare(queue=QUEUE))
    receive_unwrap(
        server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG, no_wait=True)
    )

    assert not subsets_any_sendable(server, m.BasicDeliver)
    assert not subsets_any_sendable(server, m.BasicCancel)

    send_unwrap(server, m.QueueDeclareOk)

    assert subsets_any_sendable(server, m.BasicDeliver, consumer_tag=CONSUMER_TAG)


def test_nowait_declare_after_channel_open() -> None:
    server = connected()

    receive_unwrap(server, m.ChannelOpen())
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, no_wait=True))

    assert not subsets_any_sendable(
        server, m.ConnectionClose, reply_code=mf.ReplyCode.CHANNEL_ERROR
    )

    send_unwrap(server, m.ChannelOpenOk)

    assert not subsets_any_sendable(server, m.QueueDeclareOk)


def test_pipelined_channel_open_before_connection_open_ok() -> None:
    server = tuned()

    receive_unwrap(server, m.ConnectionOpen("/"))

    assert isinstance(server.receive(m.ChannelOpen()), Unrepresentable)
