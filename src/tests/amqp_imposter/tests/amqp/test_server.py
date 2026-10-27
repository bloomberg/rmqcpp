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

import asyncio
import socket
import struct
from collections.abc import Callable
from contextlib import suppress
from dataclasses import replace

import pytest

from amqp_imposter.amqp import method_framing as fr
from amqp_imposter.amqp import session as se
from amqp_imposter.amqp.server import HOST, AmqpServer
from amqp_imposter.amqp.transcript import TranscriptEvent as ev
from amqp_imposter.model import methods as m
from amqp_imposter.model.method_weights import (
    UNPROMPTED_CONNECTION_CLOSE,
    WeightedPattern,
)

from .helpers import (
    CHANNEL,
    CONNECTION_CLOSE,
    CONNECTION_START_OK,
    CONNECTION_TUNE_OK,
    FRAME_MAX,
    GREETING_WEIGHTS,
    SCRIPTED_REPLIES,
    SEED,
    STOP_WEIGHT_DRAIN,
    encoded_method,
    poll_until,
    transcript_events,
)

ACTION_DELAY_S = 0.02
UNREAD_BYTES = 64 * 1024 * 1024


def make_config(idle_millis: int = se.DEFAULT_IDLE_MILLIS) -> se.SessionConfig:
    return se.SessionConfig(
        seed=SEED,
        stop_weight=STOP_WEIGHT_DRAIN,
        idle_millis=idle_millis,
        weights=GREETING_WEIGHTS,
        script=SCRIPTED_REPLIES,
    )


def start_only_on_idle(idle_millis: int) -> se.SessionConfig:
    return replace(
        make_config(idle_millis),
        weights=(WeightedPattern(m.ConnectionStart(), 0),),
        idle_weights=(WeightedPattern(m.ConnectionStart()),),
    )


def teardown_on_idle(idle_millis: int) -> se.SessionConfig:
    return replace(
        make_config(idle_millis),
        idle_weights=(WeightedPattern(UNPROMPTED_CONNECTION_CLOSE),),
    )


def make_amqp_server(idle_millis: int = se.DEFAULT_IDLE_MILLIS) -> AmqpServer:
    return AmqpServer(se.Session(make_config(idle_millis)))


class SlowAmqpServer(AmqpServer):
    async def do_actions(
        self, actions: tuple[se.SocketAction, ...], writer: asyncio.StreamWriter
    ) -> bool:
        await asyncio.sleep(ACTION_DELAY_S)
        return await super().do_actions(actions, writer)


async def make_amqp_client(
    port: int,
) -> tuple[asyncio.StreamReader, asyncio.StreamWriter]:
    return await asyncio.wait_for(asyncio.open_connection(HOST, port), 2.0)


async def close_writer(writer: asyncio.StreamWriter) -> None:
    writer.close()
    await writer.wait_closed()


async def egress_of(reader: asyncio.StreamReader) -> tuple[fr.Ingress, ...]:
    buffer = b""

    async with asyncio.timeout(2.0):
        while True:
            buffer += await reader.read(4096)
            items, buffer = fr.decode(buffer, FRAME_MAX)
            if items:
                return items


def make_event_seen_cb(server: AmqpServer, event: ev) -> Callable[[], bool]:
    return lambda: event in transcript_events(server.session.transcript())


async def amqp_handshake(writer: asyncio.StreamWriter) -> None:
    writer.write(fr.PROTOCOL_HEADER_091)
    writer.write(encoded_method(CONNECTION_START_OK))
    writer.write(encoded_method(CONNECTION_TUNE_OK))
    writer.write(encoded_method(m.ConnectionOpen()))
    await writer.drain()


async def decode_until_eof(reader: asyncio.StreamReader) -> tuple[fr.Ingress, ...]:
    buffer = b""

    async with asyncio.timeout(2.0):
        while chunk := await reader.read(4096):
            buffer += chunk

    items, _ = fr.decode(buffer, FRAME_MAX)
    return items


async def test_start_ephemeral_port() -> None:
    server = make_amqp_server()

    port = await server.start()

    assert port > 0
    assert server.port == port

    await server.stop()


async def test_start_requested_port() -> None:
    first = make_amqp_server()
    port = await first.start()
    await first.stop()
    server = make_amqp_server()

    assert await server.start(port=port) == port

    await server.stop()


async def test_refuse_client_after_stop() -> None:
    server = make_amqp_server()
    port = await server.start()

    await server.stop()

    with pytest.raises(ConnectionRefusedError):
        await make_amqp_client(port)


async def test_drop_client_on_stop() -> None:
    server = make_amqp_server()
    port = await server.start()
    reader, _ = await make_amqp_client(port)
    await poll_until(make_event_seen_cb(server, ev.TCP_CONNECTED))

    await server.stop()

    assert await asyncio.wait_for(reader.read(), 2.0) == b""


async def test_stop_while_client_reconnects() -> None:
    server = make_amqp_server()
    port = await server.start()
    writers: list[asyncio.StreamWriter] = []

    async def keep_connecting() -> None:
        while True:
            with suppress(OSError):
                _, writer = await asyncio.open_connection(HOST, port)
                writers.append(writer)
            await asyncio.sleep(0)

    connecting = asyncio.create_task(keep_connecting())
    await poll_until(lambda: server.writer is not None)

    await asyncio.wait_for(server.stop(), 2.0)

    connecting.cancel()
    for writer in writers:
        writer.close()


async def test_disconnect_hung_client() -> None:
    server = make_amqp_server()
    port = await server.start()
    _, writer = await make_amqp_client(port)
    await poll_until(lambda: server.writer is not None)
    assert server.writer is not None
    server.writer.write(bytes(UNREAD_BYTES))

    await asyncio.wait_for(server.disconnect(), 2.0)

    assert server.writer is None

    await close_writer(writer)
    await server.stop()


async def test_rejecting_connections() -> None:
    server = make_amqp_server()
    port = await server.start()

    async with server.rejecting_connections():
        reader, refused = await make_amqp_client(port)

        assert await asyncio.wait_for(reader.read(), 2.0) == b""
        assert ev.TCP_CONNECTED not in transcript_events(server.session.transcript())

    _, accepted = await make_amqp_client(port)
    await poll_until(make_event_seen_cb(server, ev.TCP_CONNECTED))

    await close_writer(refused)
    await close_writer(accepted)
    await server.stop()


async def test_connect() -> None:
    server = make_amqp_server()
    port = await server.start()

    _, writer = await make_amqp_client(port)
    await poll_until(make_event_seen_cb(server, ev.TCP_CONNECTED))

    await close_writer(writer)
    await server.stop()


async def test_client_close() -> None:
    server = make_amqp_server()
    port = await server.start()
    _, writer = await make_amqp_client(port)
    await poll_until(make_event_seen_cb(server, ev.TCP_CONNECTED))

    await close_writer(writer)
    await poll_until(make_event_seen_cb(server, ev.TCP_DOWN))

    await server.stop()


async def test_greeting() -> None:
    server = make_amqp_server()
    port = await server.start()
    reader, writer = await make_amqp_client(port)

    writer.write(fr.PROTOCOL_HEADER_091)
    await writer.drain()

    assert [type(one) for one in await egress_of(reader)] == [fr.MethodOnChannel]

    await close_writer(writer)
    await server.stop()


async def test_second_client_rejected() -> None:
    server = make_amqp_server()
    port = await server.start()
    _, first = await make_amqp_client(port)
    await poll_until(make_event_seen_cb(server, ev.TCP_CONNECTED))

    reader, writer = await make_amqp_client(port)
    await poll_until(make_event_seen_cb(server, ev.TCP_REJECTED))

    assert await asyncio.wait_for(reader.read(), 2.0) == b""

    await close_writer(writer)
    await close_writer(first)
    await server.stop()


async def test_drop_closes_connection() -> None:
    server = make_amqp_server()
    port = await server.start()
    reader, writer = await make_amqp_client(port)

    writer.write(encoded_method(CONNECTION_START_OK))
    await writer.drain()

    assert await asyncio.wait_for(reader.read(), 2.0) == b""

    await server.stop()


async def test_client_reset() -> None:
    loop = asyncio.get_running_loop()
    unhandled: list[dict[str, object]] = []
    loop.set_exception_handler(lambda _, context: unhandled.append(context))
    server = make_amqp_server()
    port = await server.start()
    _, writer = await make_amqp_client(port)
    await poll_until(make_event_seen_cb(server, ev.TCP_CONNECTED))

    writer.get_extra_info("socket").setsockopt(
        socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0)
    )
    writer.transport.abort()
    await poll_until(make_event_seen_cb(server, ev.TCP_DOWN))
    await asyncio.sleep(0)

    assert unhandled == []

    await server.stop()


async def test_reconnect_after_close() -> None:
    server = make_amqp_server()
    port = await server.start()
    _, first = await make_amqp_client(port)
    await poll_until(make_event_seen_cb(server, ev.TCP_CONNECTED))
    await close_writer(first)
    await poll_until(make_event_seen_cb(server, ev.TCP_DOWN))

    _, writer = await make_amqp_client(port)
    await poll_until(lambda: server.writer is not None)

    assert ev.TCP_REJECTED not in transcript_events(server.session.transcript())

    await close_writer(writer)
    await server.stop()


async def test_idling() -> None:
    server = AmqpServer(se.Session(start_only_on_idle(idle_millis=10)))
    port = await server.start()
    _, writer = await make_amqp_client(port)
    writer.write(fr.PROTOCOL_HEADER_091)
    await writer.drain()

    await poll_until(make_event_seen_cb(server, ev.IDLE_TIMEOUT))

    await close_writer(writer)
    await server.stop()


async def test_method_ingress_resets_idle_timer() -> None:
    server = AmqpServer(se.Session(teardown_on_idle(idle_millis=500)))
    port = await server.start()

    _, writer = await make_amqp_client(port)
    await amqp_handshake(writer)
    writer.write(encoded_method(m.ChannelOpen(), CHANNEL))
    await writer.drain()

    for _ in range(18):
        writer.write(encoded_method(m.BasicQos(), CHANNEL))
        await writer.drain()
        await asyncio.sleep(0.01)

    assert ev.IDLE_TIMEOUT not in transcript_events(server.session.transcript())
    await poll_until(make_event_seen_cb(server, ev.IDLE_TIMEOUT))

    await close_writer(writer)
    await server.stop()


async def test_heartbeat_does_not_reset_idle_timer() -> None:
    server = AmqpServer(se.Session(teardown_on_idle(idle_millis=60)))
    port = await server.start()

    _, writer = await make_amqp_client(port)
    await amqp_handshake(writer)
    heartbeat = fr.encode(fr.Heartbeat(), FRAME_MAX)

    for _ in range(20):
        writer.write(heartbeat)
        await writer.drain()
        await asyncio.sleep(0.01)

    assert ev.IDLE_TIMEOUT in transcript_events(server.session.transcript())

    await close_writer(writer)
    await server.stop()


async def test_slow_egress_does_not_starve_ingress() -> None:
    server = SlowAmqpServer(se.Session(start_only_on_idle(idle_millis=1)))
    port = await server.start()
    _, writer = await make_amqp_client(port)
    writer.write(fr.PROTOCOL_HEADER_091)
    await writer.drain()
    await poll_until(make_event_seen_cb(server, ev.IDLE_TIMEOUT))

    writer.write(encoded_method(CONNECTION_START_OK))
    await writer.drain()

    await poll_until(make_event_seen_cb(server, ev.METHOD_INGRESS))

    await close_writer(writer)
    await server.stop()


async def test_flush_on_drop() -> None:
    server = make_amqp_server()
    port = await server.start()
    reader, writer = await make_amqp_client(port)

    await amqp_handshake(writer)
    writer.write(encoded_method(CONNECTION_CLOSE))
    await writer.drain()

    methods = [
        type(item.method)
        for item in await decode_until_eof(reader)
        if isinstance(item, fr.MethodOnChannel)
    ]

    assert m.ConnectionCloseOk in methods

    await close_writer(writer)
    await server.stop()
