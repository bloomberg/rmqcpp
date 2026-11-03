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
from collections.abc import AsyncGenerator
from contextlib import asynccontextmanager, suppress
from dataclasses import dataclass, field

from .session import Drop, Session, SocketAction, Write

HOST = "127.0.0.1"
EPHEMERAL_PORT = 0
READ_BYTES = 64 * 1024


@dataclass(slots=True)
class AmqpServer:
    session: Session

    server: asyncio.Server | None = field(default=None, init=False)
    writer: asyncio.StreamWriter | None = field(default=None, init=False)
    connection: asyncio.Task[None] | None = field(default=None, init=False)
    accepting_connections: bool = field(default=True, init=False)
    port: int | None = field(default=None, init=False)

    async def start(self, port: int = EPHEMERAL_PORT, host: str = HOST) -> int:
        self.server = await asyncio.start_server(self.serve, host, port)
        self.port = int(self.server.sockets[0].getsockname()[1])
        return self.port

    async def stop(self) -> None:
        async with self.rejecting_connections():
            if self.server is not None:
                self.server.close()

            await self.disconnect()

            if self.server is not None:
                await self.server.wait_closed()
                self.server = None

    async def disconnect(self) -> None:
        connection = self.connection

        if self.writer is not None:
            self.writer.transport.abort()

        if connection is not None:
            await asyncio.wait((connection,))

    @asynccontextmanager
    async def rejecting_connections(self) -> AsyncGenerator[None]:
        self.accepting_connections = False
        try:
            yield
        finally:
            self.accepting_connections = True

    async def shutdown(self, writer: asyncio.StreamWriter) -> None:
        with suppress(OSError):
            writer.close()
            await writer.wait_closed()

    async def do_actions(
        self, actions: tuple[SocketAction, ...], writer: asyncio.StreamWriter
    ) -> bool:
        for action in actions:
            match action:
                case Write(payload):
                    writer.write(payload)
                case Drop():
                    with suppress(OSError):
                        await writer.drain()
                    return True

        with suppress(OSError):
            await writer.drain()

        return False

    async def timebound_read(
        self, reader: asyncio.StreamReader, time_s: float
    ) -> bytes | None:
        if time_s <= 0.0:
            return None

        try:
            async with asyncio.timeout(time_s):
                return await reader.read(READ_BYTES)
        except TimeoutError:
            return None
        except OSError:
            return b""  # EOF

    async def loop(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        loop = asyncio.get_running_loop()
        idle = self.session.config.idle_millis / 1_000.0
        deadline = loop.time() + idle

        while True:
            match await self.timebound_read(reader, deadline - loop.time()):
                case None:
                    actions = self.session.on_idle()
                    refreshed = True
                case b"":  # EOF
                    return
                case chunk:
                    before = self.session.total_ingress_methods
                    actions = self.session.on_ingress(chunk)
                    refreshed = self.session.total_ingress_methods > before

            if await self.do_actions(actions, writer):
                return

            if refreshed:
                deadline = loop.time() + idle

    async def serve(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        if not self.accepting_connections:
            await self.shutdown(writer)
            return

        if self.writer is not None:
            await self.do_actions(self.session.on_rejected(), writer)
            await self.shutdown(writer)
            return

        self.writer = writer
        self.connection = asyncio.current_task()
        await self.do_actions(self.session.on_connected(), writer)

        try:
            await self.loop(reader, writer)
        finally:
            self.writer = None
            self.connection = None
            self.session.on_disconnected()
            await self.shutdown(writer)
