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
import json
import socket
import stat
import sys
from pathlib import Path
from typing import Any

import pytest

from amqp_imposter.amqp.server import EPHEMERAL_PORT, HOST
from amqp_imposter.control import cli
from amqp_imposter.control import transport as tp
from amqp_imposter.control.v1 import imposter_pb2 as pb
from amqp_imposter.control.v1 import imposter_pb2_grpc as pb_grpc

from .helpers import channel_for

STARTUP_TIMEOUT = 5.0
SOCK_PATH_SPECIFIED = "/tmp/given.sock"


async def cli_start_imposter(*arguments: str) -> asyncio.subprocess.Process:
    return await asyncio.create_subprocess_exec(
        sys.executable,
        "-m",
        "amqp_imposter.control",
        *arguments,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
    )


async def read_announcement(process: asyncio.subprocess.Process) -> dict[str, Any]:
    assert process.stdout is not None

    line = await asyncio.wait_for(process.stdout.readline(), STARTUP_TIMEOUT)
    assert line, "not ready"

    document: dict[str, Any] = json.loads(line)
    return document


async def terminate(process: asyncio.subprocess.Process) -> None:
    process.terminate()
    await asyncio.wait_for(process.wait(), STARTUP_TIMEOUT)


def test_sock_flag() -> None:
    assert cli.parse_args(["--socket", SOCK_PATH_SPECIFIED]).socket == Path(
        SOCK_PATH_SPECIFIED
    )


def test_sock_default() -> None:
    assert cli.parse_args([]).socket is None


def test_amqp_port_flag() -> None:
    assert cli.parse_args(["--amqp-port", "5672"]).amqp_port == 5672


def test_amqp_port_default() -> None:
    assert cli.parse_args([]).amqp_port == EPHEMERAL_PORT


@pytest.mark.parametrize(
    "argv",
    (
        ["bogus"],
        ["--amqp-port", "nope"],
        ["--amqp-port"],
        ["--socket"],
        ["--nope"],
        ["serve"],
    ),
)
def test_usage_errors(argv: list[str]) -> None:
    with pytest.raises(SystemExit) as e:
        cli.parse_args(argv)

    assert e.value.code == cli.ReturnCodes.USAGE_ERROR


async def test_readiness_announcement(socket_path: Path) -> None:
    process = await cli_start_imposter("--socket", str(socket_path))

    announcement = await read_announcement(process)

    assert set(announcement) == {"socket", "amqp_port"}
    assert announcement["socket"] == str(socket_path)
    assert isinstance(announcement["amqp_port"], int)

    await terminate(process)


async def test_usable_when_ready(socket_path: Path) -> None:
    process = await cli_start_imposter("--socket", str(socket_path))
    await read_announcement(process)

    channel = channel_for(socket_path)
    reply = await pb_grpc.ImposterStub(channel).Status(pb.StatusRequest())

    assert reply.transcript_lines > 0

    await channel.close(None)
    await terminate(process)


async def test_announced_port_is_bound(socket_path: Path) -> None:
    process = await cli_start_imposter("--socket", str(socket_path), "--amqp-port", "0")
    announcement = await read_announcement(process)

    channel = channel_for(socket_path)
    reply = await pb_grpc.ImposterStub(channel).Status(pb.StatusRequest())

    assert reply.amqp_port == announcement["amqp_port"]

    await channel.close(None)
    await terminate(process)


async def test_sock_removed_on_signal(socket_path: Path) -> None:
    process = await cli_start_imposter("--socket", str(socket_path))
    await read_announcement(process)

    await terminate(process)

    assert not await asyncio.to_thread(socket_path.exists)


async def test_sock_announce() -> None:
    process = await cli_start_imposter()
    announcement = await read_announcement(process)

    socket_path = Path(announcement["socket"])

    assert socket_path.name == tp.CONTROL_SOCK_NAME
    assert socket_path.parent.parent == tp.CONTROL_SOCK_DIR_PARENT
    assert socket_path.parent.name.startswith("amqp-imposter-")

    await terminate(process)


async def test_private_sock_dir() -> None:
    process = await cli_start_imposter()
    announcement = await read_announcement(process)
    directory = Path(announcement["socket"]).parent

    mode = await asyncio.to_thread(lambda: directory.stat().st_mode)

    assert stat.S_IMODE(mode) == 0o700

    await terminate(process)


async def test_generated_sock_cleaned_up() -> None:
    process = await cli_start_imposter()
    announcement = await read_announcement(process)
    directory = Path(announcement["socket"]).parent

    await terminate(process)

    assert not await asyncio.to_thread(directory.exists)


async def test_specified_sock_not_cleaned_up(socket_path: Path) -> None:
    process = await cli_start_imposter("--socket", str(socket_path))
    await read_announcement(process)

    await terminate(process)

    assert await asyncio.to_thread(socket_path.parent.exists)


async def test_sock_taken(socket_path: Path) -> None:
    process = await cli_start_imposter("--socket", str(socket_path))
    await read_announcement(process)

    intruder = await cli_start_imposter("--socket", str(socket_path))
    code = await asyncio.wait_for(intruder.wait(), STARTUP_TIMEOUT)

    assert code == cli.ReturnCodes.RUNTIME_ERROR
    assert await asyncio.to_thread(socket_path.exists)

    await terminate(process)


def bind_stale_sock(sock_path: Path) -> None:
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.bind(str(sock_path))


async def test_stale_sock_replaced(socket_path: Path) -> None:
    await asyncio.to_thread(bind_stale_sock, socket_path)

    process = await cli_start_imposter("--socket", str(socket_path))
    announcement = await read_announcement(process)

    assert announcement["socket"] == str(socket_path)

    await terminate(process)


async def test_keep_regular_file_at_sock_path(socket_path: Path) -> None:
    await asyncio.to_thread(socket_path.write_text, "not a socket")

    process = await cli_start_imposter("--socket", str(socket_path))
    code = await asyncio.wait_for(process.wait(), STARTUP_TIMEOUT)

    assert code == cli.ReturnCodes.RUNTIME_ERROR
    assert await asyncio.to_thread(socket_path.read_text) == "not a socket"


async def test_keep_directory_at_sock_path(socket_path: Path) -> None:
    await asyncio.to_thread(socket_path.mkdir)

    process = await cli_start_imposter("--socket", str(socket_path))
    code = await asyncio.wait_for(process.wait(), STARTUP_TIMEOUT)

    assert code == cli.ReturnCodes.RUNTIME_ERROR
    assert await asyncio.to_thread(socket_path.is_dir)


async def test_amqp_port_taken(socket_path: Path) -> None:
    with socket.socket() as taken:
        taken.bind((HOST, 0))
        taken.listen()
        port = str(taken.getsockname()[1])

        intruder = await cli_start_imposter(
            "--socket", str(socket_path), "--amqp-port", port
        )
        code = await asyncio.wait_for(intruder.wait(), STARTUP_TIMEOUT)

    assert code == cli.ReturnCodes.RUNTIME_ERROR
    assert not await asyncio.to_thread(socket_path.exists)
