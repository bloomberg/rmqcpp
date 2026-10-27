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

from collections.abc import AsyncGenerator
from contextlib import asynccontextmanager
from pathlib import Path

import grpc
import pytest
from grpc import aio
from grpc_reflection.v1alpha import reflection_pb2, reflection_pb2_grpc

from amqp_imposter.amqp.session import DEFAULT_SESSION_CONFIG
from amqp_imposter.control import service as sv
from amqp_imposter.control.v1 import imposter_pb2 as pb
from amqp_imposter.control.v1 import imposter_pb2_grpc as pb_grpc
from amqp_imposter.imposter import Imposter

from .helpers import channel_for, make_imposter

SEED = 7
LONG_TRANSCRIPT = 2000


@asynccontextmanager
async def running(imposter: Imposter, path: Path) -> AsyncGenerator[aio.Channel]:
    server = await sv.serve(path, imposter)
    channel = channel_for(path)
    try:
        yield channel
    finally:
        await channel.close(None)
        await server.stop(None)


async def transcript_lines(channel: aio.Channel) -> tuple[pb.TranscriptLine, ...]:
    stub = pb_grpc.ImposterStub(channel)
    return tuple([line async for line in stub.Transcript(pb.TranscriptRequest())])


async def test_status_compose_from_to(socket_path: Path) -> None:
    async with running(make_imposter(), socket_path) as channel:
        reply = await pb_grpc.ImposterStub(channel).Status(pb.StatusRequest())

        assert reply.transcript_lines == 1


async def test_configure_compose_from_to(socket_path: Path) -> None:
    imposter = make_imposter()

    async with running(imposter, socket_path) as channel:
        reply = await pb_grpc.ImposterStub(channel).Configure(
            pb.ConfigureRequest(config=pb.SessionConfig(seed=SEED))
        )

        assert reply.config.seed == SEED
        assert imposter.session.config.seed == SEED


async def test_reset_compose_from_to(socket_path: Path) -> None:
    imposter = make_imposter()

    async with running(imposter, socket_path) as channel:
        stub = pb_grpc.ImposterStub(channel)
        await stub.Configure(pb.ConfigureRequest(config=pb.SessionConfig(seed=SEED)))
        await stub.Reset(pb.ResetRequest())

        assert imposter.session.config == DEFAULT_SESSION_CONFIG


async def test_transcript_stream(socket_path: Path) -> None:
    imposter = make_imposter()
    imposter.session.on_connected()

    async with running(imposter, socket_path) as channel:
        lines = await transcript_lines(channel)

        assert [line.index for line in lines] == [
            line.index for line in imposter.session.transcript()
        ]


async def test_long_transcript_stream(socket_path: Path) -> None:
    imposter = make_imposter()

    for _ in range(LONG_TRANSCRIPT):
        imposter.session.on_connected()
        imposter.session.on_disconnected()

    async with running(imposter, socket_path) as channel:
        lines = await transcript_lines(channel)

        assert len(lines) > LONG_TRANSCRIPT


async def test_bad_configure_is_invalid_argument(socket_path: Path) -> None:
    imposter = make_imposter()

    async with running(imposter, socket_path) as channel:
        with pytest.raises(aio.AioRpcError) as e:
            await pb_grpc.ImposterStub(channel).Configure(
                pb.ConfigureRequest(config=pb.SessionConfig(seed=SEED, stop_weight=0))
            )

        assert e.value.code() == grpc.StatusCode.INVALID_ARGUMENT
        assert imposter.session.config == DEFAULT_SESSION_CONFIG


async def test_no_imposter_is_unavailable(socket_path: Path) -> None:
    channel = channel_for(socket_path)

    with pytest.raises(aio.AioRpcError) as e:
        await pb_grpc.ImposterStub(channel).Status(pb.StatusRequest())

    assert e.value.code() == grpc.StatusCode.UNAVAILABLE

    await channel.close(None)


async def test_concurrent_clients(socket_path: Path) -> None:
    async with running(make_imposter(), socket_path) as one:
        other = channel_for(socket_path)

        await pb_grpc.ImposterStub(one).Configure(
            pb.ConfigureRequest(config=pb.SessionConfig(seed=SEED))
        )
        reply = await pb_grpc.ImposterStub(other).Status(pb.StatusRequest())

        assert reply.config.seed == SEED

        await other.close(None)


async def test_independent_servers(socket_path: Path) -> None:
    other_path = socket_path.with_name("other.sock")

    async with (
        running(make_imposter(), socket_path) as one,
        running(make_imposter(), other_path) as other,
    ):
        await pb_grpc.ImposterStub(one).Configure(
            pb.ConfigureRequest(config=pb.SessionConfig(seed=SEED))
        )
        reply = await pb_grpc.ImposterStub(other).Status(pb.StatusRequest())

        assert reply.config.seed != SEED


async def test_reflection_lists_the_service(socket_path: Path) -> None:
    async with running(make_imposter(), socket_path) as channel:
        stub = reflection_pb2_grpc.ServerReflectionStub(channel)
        request = reflection_pb2.ServerReflectionRequest(list_services="")

        services = {
            service.name
            async for response in stub.ServerReflectionInfo(iter([request]))
            for service in response.list_services_response.service
        }

        assert pb.DESCRIPTOR.services_by_name["Imposter"].full_name in services
