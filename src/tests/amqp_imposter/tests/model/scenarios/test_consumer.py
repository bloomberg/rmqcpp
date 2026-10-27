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

from amqp_imposter.model import methods as m

from ..helpers import (
    CONSUMER_TAG,
    OTHER_CONSUMER_TAG,
    QUEUE,
    channelled,
    consume,
    consuming,
    deliver,
    lookup_unique_sendable,
    message_content,
    receive_unwrap,
    reopen_channel,
    send_channel_error,
    send_unwrap,
    subsets_any_sendable,
)


def test_example_consumer() -> None:
    server = channelled()

    receive_unwrap(server, m.BasicQos(prefetch_count=2))
    send_unwrap(server, m.BasicQosOk)
    receive_unwrap(server, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG))

    assert not subsets_any_sendable(server, m.BasicDeliver)
    assert not subsets_any_sendable(server, m.BasicCancel)

    send_unwrap(server, m.BasicConsumeOk)

    assert lookup_unique_sendable(server, m.BasicCancel).consumer_tag == CONSUMER_TAG
    assert lookup_unique_sendable(server, m.BasicDeliver).delivery_tag == 1

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)

    assert lookup_unique_sendable(server, m.BasicDeliver).delivery_tag == 2

    send_unwrap(server, m.BasicDeliver, delivery_tag=2)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicAck(delivery_tag=1))

    send_unwrap(server, m.BasicDeliver, delivery_tag=3)
    receive_unwrap(server, m.BasicQos(prefetch_count=1))
    send_unwrap(server, m.BasicQosOk)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicAck(delivery_tag=2))

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicAck(delivery_tag=3))

    assert lookup_unique_sendable(server, m.BasicDeliver).delivery_tag == 4


def test_example_resilient_consumer() -> None:
    server = consuming()

    deliver(server, 1)
    receive_unwrap(server, m.BasicAck(delivery_tag=1))
    deliver(server, 2)

    # no wait, cancel is effective immediately
    send_unwrap(server, m.BasicCancel, consumer_tag=CONSUMER_TAG, no_wait=True)
    receive_unwrap(server, m.BasicAck(delivery_tag=2))
    consume(server)

    third = deliver(server, 3)
    receive_unwrap(server, m.BasicAck(delivery_tag=2))  # double ack
    send_channel_error(server)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.ChannelCloseOk())
    reopen_channel(server)
    receive_unwrap(server, m.BasicQos(prefetch_count=1))
    send_unwrap(server, m.BasicQosOk)
    consume(server)

    redelivery = lookup_unique_sendable(server, m.BasicDeliver, redelivered=True)

    assert message_content(redelivery) == message_content(third)
    assert redelivery.delivery_tag == 1

    server.send(redelivery)
    receive_unwrap(server, m.BasicAck(delivery_tag=1))


def test_example_reconsumer() -> None:
    server = consuming()

    send_unwrap(server, m.BasicDeliver, delivery_tag=1)
    receive_unwrap(server, m.BasicAck(delivery_tag=1))
    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG))
    send_unwrap(server, m.BasicCancelOk, consumer_tag=CONSUMER_TAG)

    assert not subsets_any_sendable(server, m.BasicDeliver)

    receive_unwrap(server, m.BasicConsume(queue=QUEUE, consumer_tag=OTHER_CONSUMER_TAG))
    send_unwrap(server, m.BasicConsumeOk, consumer_tag=OTHER_CONSUMER_TAG)

    assert not subsets_any_sendable(server, m.BasicDeliver, consumer_tag=CONSUMER_TAG)

    delivery = lookup_unique_sendable(server, m.BasicDeliver)

    assert delivery.consumer_tag == OTHER_CONSUMER_TAG
    assert delivery.delivery_tag == 2

    server.send(delivery)
    receive_unwrap(server, m.BasicCancel(consumer_tag=OTHER_CONSUMER_TAG))
    send_unwrap(server, m.BasicCancelOk, consumer_tag=OTHER_CONSUMER_TAG)

    assert not subsets_any_sendable(server, m.BasicDeliver)
    assert not subsets_any_sendable(server, m.BasicCancel)

    receive_unwrap(server, m.BasicAck(delivery_tag=2))
