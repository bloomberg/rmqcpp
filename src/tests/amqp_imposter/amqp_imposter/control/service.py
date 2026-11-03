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

import time
from collections.abc import AsyncIterator
from pathlib import Path

import grpc
from grpc import aio
from grpc_reflection.v1alpha import reflection

from ..imposter import Imposter, ImposterError
from . import config_proto as cp
from . import method_proto as mp
from . import transport as tp
from .v1 import imposter_pb2 as pb
from .v1 import imposter_pb2_grpc as pb_grpc

GRPC_SERVICE = "Imposter"


def now_micros() -> int:
    return time.time_ns() // 1_000


def status_reply(imposter: Imposter, epoch_micros: int) -> pb.StatusReply:
    reply = pb.StatusReply(
        amqp_greeted=imposter.session.is_greeted(),
        transcript_lines=imposter.session.lines.recorded,
        epoch_micros=epoch_micros,
        config=cp.to_proto(imposter.session.config),
    )

    if imposter.amqp_server.port is not None:
        reply.amqp_port = imposter.amqp_server.port

    return reply


async def configure(imposter: Imposter, message: pb.SessionConfig) -> None:
    config = cp.from_proto(message)

    async with imposter.amqp_server.rejecting_connections():
        await imposter.amqp_server.disconnect()
        imposter.session.reset(config)


class ImposterServicer(pb_grpc.ImposterServicer):
    def __init__(self, imposter: Imposter) -> None:
        self.imposter = imposter

    async def reconfigured(
        self, message: pb.SessionConfig, context: aio.ServicerContext[object, object]
    ) -> pb.StatusReply:
        try:
            await configure(self.imposter, message)
        except ImposterError as bad:
            await context.abort(grpc.StatusCode.INVALID_ARGUMENT, str(bad))

        return status_reply(self.imposter, now_micros())

    async def Configure(
        self, request: pb.ConfigureRequest, context: aio.ServicerContext[object, object]
    ) -> pb.StatusReply:
        return await self.reconfigured(request.config, context)

    async def Status(
        self, request: pb.StatusRequest, context: aio.ServicerContext[object, object]
    ) -> pb.StatusReply:
        return status_reply(self.imposter, now_micros())

    async def Reset(
        self, request: pb.ResetRequest, context: aio.ServicerContext[object, object]
    ) -> pb.StatusReply:
        return await self.reconfigured(pb.SessionConfig(), context)

    async def Transcript(
        self,
        request: pb.TranscriptRequest,
        context: aio.ServicerContext[object, object],
    ) -> AsyncIterator[pb.TranscriptLine]:
        for line in self.imposter.session.transcript():
            yield mp.line_to_proto(line)


async def serve(path: Path, imposter: Imposter) -> aio.Server:
    server = aio.server(options=tp.GRPC_CHANNEL_OPTS)
    pb_grpc.add_ImposterServicer_to_server(ImposterServicer(imposter), server)
    reflection.enable_server_reflection(
        (
            pb.DESCRIPTOR.services_by_name[GRPC_SERVICE].full_name,
            reflection.SERVICE_NAME,
        ),
        server,
    )
    server.add_insecure_port(tp.uds_target(path))

    await server.start()

    return server
