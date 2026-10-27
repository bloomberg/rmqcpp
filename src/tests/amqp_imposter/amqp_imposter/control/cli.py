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

import argparse
import asyncio
import shutil
import signal
import socket
import stat
import sys
import tempfile
from collections.abc import Generator
from contextlib import contextmanager, suppress
from enum import IntEnum
from pathlib import Path

from ..amqp.server import EPHEMERAL_PORT, AmqpServer
from ..amqp.session import Session
from ..imposter import Imposter
from . import service as sv
from . import transport as tp


class ReturnCodes(IntEnum):
    OK = 0
    RUNTIME_ERROR = 1
    USAGE_ERROR = 2


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="amqp-imposter",
        description="Foreground an AMQP 0-9-1 imposter and its gRPC control socket.",
    )
    parser.add_argument("--socket", type=Path, default=None)
    parser.add_argument("--amqp-port", type=int, default=EPHEMERAL_PORT)

    return parser.parse_args(argv)


def sock_write_init(sock_path: Path, amqp_port: int) -> None:
    sys.stdout.write(tp.ImposterEndpoints(sock_path, amqp_port).to_json_line())
    sys.stdout.flush()


def sock_unlink(sock_path: Path) -> None:
    with suppress(OSError):
        sock_path.unlink(missing_ok=True)


def sock_acquire(sock_path: Path) -> bool:
    try:
        mode = sock_path.lstat().st_mode
    except FileNotFoundError:
        return True

    if not stat.S_ISSOCK(mode):
        return False

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        try:
            sock.connect(str(sock_path))
        except ConnectionRefusedError:
            sock_unlink(sock_path)
            return True

    return False


@contextmanager
def control_sock(custom_sock_path: Path | None) -> Generator[Path]:
    sock_path = (
        Path(tempfile.mkdtemp(prefix="amqp-imposter-", dir=tp.CONTROL_SOCK_DIR_PARENT))
        / tp.CONTROL_SOCK_NAME
        if custom_sock_path is None
        else custom_sock_path
    )
    if not sock_acquire(sock_path):
        raise OSError(f"control socket path unavailable: {sock_path}")

    try:
        yield sock_path
    finally:
        sock_unlink(sock_path)

        if custom_sock_path is None:
            shutil.rmtree(sock_path.parent, ignore_errors=True)


async def run(custom_sock_path: Path | None, amqp_port: int) -> int:
    session = Session()
    imposter = Imposter(session=session, amqp_server=AmqpServer(session))

    try:
        port = await imposter.amqp_server.start(port=amqp_port)
    except OSError:
        return ReturnCodes.RUNTIME_ERROR

    try:
        with control_sock(custom_sock_path) as sock_path:
            server = await sv.serve(sock_path, imposter)
            stopping = asyncio.Event()

            loop = asyncio.get_running_loop()
            loop.add_signal_handler(signal.SIGINT, stopping.set)
            loop.add_signal_handler(signal.SIGTERM, stopping.set)

            sock_write_init(sock_path, port)
            try:
                await stopping.wait()
            finally:
                await server.stop(None)
    except OSError:
        return ReturnCodes.RUNTIME_ERROR
    finally:
        await imposter.amqp_server.stop()

    return ReturnCodes.OK


def main(argv: list[str] | None = None) -> int:
    parsed = parse_args(argv)
    return asyncio.run(run(parsed.socket, parsed.amqp_port))
