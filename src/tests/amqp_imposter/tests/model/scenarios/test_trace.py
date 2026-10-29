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

from amqp_imposter.model import ModelServer, NegotiationOptions
from amqp_imposter.model import methods as m
from amqp_imposter.model.constants import CHANNEL_MAX_CEIL

from ..helpers import (
    CONSUMER_TAG,
    EXCHANGE,
    QUEUE,
    ROUTING_KEY,
    close_channel_from_client,
    close_connection_from_client,
    close_connection_from_server,
    consume,
    publish,
    receive_unwrap,
    send_unwrap,
    start_ok_of,
)

PREFETCH_COUNT = 10
NEGOTIATION_OPTIONS = NegotiationOptions(channel_max=CHANNEL_MAX_CEIL)


def traced_connect(server: ModelServer) -> None:
    server.receive_protocol_header()
    receive_unwrap(server, start_ok_of(send_unwrap(server, m.ConnectionStart)))

    tune = send_unwrap(server, m.ConnectionTune)

    receive_unwrap(
        server, m.ConnectionTuneOk(CHANNEL_MAX_CEIL, tune.frame_max, tune.heartbeat)
    )
    receive_unwrap(server, m.ConnectionOpen())
    send_unwrap(server, m.ConnectionOpenOk)
    receive_unwrap(server, m.ChannelOpen())
    send_unwrap(server, m.ChannelOpenOk)


def traced_declare(server: ModelServer) -> None:
    receive_unwrap(server, m.QueueDeclare(queue=QUEUE, durable=True))
    send_unwrap(server, m.QueueDeclareOk, queue=QUEUE)
    receive_unwrap(
        server,
        m.ExchangeDeclare(exchange=EXCHANGE, exchange_type="direct", durable=True),
    )
    send_unwrap(server, m.ExchangeDeclareOk)
    receive_unwrap(
        server, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, routing_key=ROUTING_KEY)
    )
    send_unwrap(server, m.QueueBindOk)


def traced_produce(server: ModelServer) -> None:
    receive_unwrap(server, m.ConfirmSelect())
    send_unwrap(server, m.ConfirmSelectOk)
    publish(server, mandatory=True)
    send_unwrap(server, m.BasicAck, delivery_tag=1, multiple=False)


def traced_consume(server: ModelServer) -> None:
    receive_unwrap(server, m.BasicQos(prefetch_count=PREFETCH_COUNT))
    send_unwrap(server, m.BasicQosOk)
    consume(server)


def traced_teardown(server: ModelServer) -> None:
    close_channel_from_client(server)
    close_connection_from_client(server)


def test_traced_producer() -> None:
    server = ModelServer(NEGOTIATION_OPTIONS)

    traced_connect(server)
    traced_declare(server)
    traced_produce(server)

    close_connection_from_server(server)

    traced_connect(server)
    traced_declare(server)
    traced_produce(server)
    traced_teardown(server)

    assert server.sendables() == ()


def test_traced_consumer() -> None:
    server = ModelServer(NEGOTIATION_OPTIONS)

    traced_connect(server)
    traced_declare(server)
    traced_consume(server)

    close_connection_from_server(server)

    traced_connect(server)
    traced_declare(server)
    traced_consume(server)

    receive_unwrap(server, m.BasicCancel(consumer_tag=CONSUMER_TAG))
    send_unwrap(server, m.BasicCancelOk, consumer_tag=CONSUMER_TAG)
    traced_teardown(server)

    assert server.sendables() == ()
