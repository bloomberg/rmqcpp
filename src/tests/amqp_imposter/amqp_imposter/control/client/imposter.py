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

import select
import subprocess
import sys
import tempfile
from collections.abc import Generator
from contextlib import contextmanager, suppress
from dataclasses import dataclass
from pathlib import Path

import grpc

from .. import transport as tp
from ..transport import ImposterEndpoints
from ..v1 import imposter_pb2 as pb
from ..v1 import imposter_pb2_grpc as pb_grpc
from .transcript_util import Transcript

TIMEOUT_STARTUP = 10.0
TIMEOUT_CONTROL = 10.0
TIMEOUT_SHUTDOWN = 10.0

IMPOSTER_MODULE = "amqp_imposter.control"


class ImposterStartupError(RuntimeError):
    pass


class ImposterShutdownError(RuntimeError):
    pass


@dataclass(frozen=True, slots=True)
class AmqpImposter:
    endpoints: ImposterEndpoints
    stub: pb_grpc.ImposterStub

    @property
    def amqp_port(self) -> int:
        return self.endpoints.amqp_port

    def configure(self, config: pb.SessionConfig) -> pb.StatusReply:
        return self.stub.Configure(
            pb.ConfigureRequest(config=config), timeout=TIMEOUT_CONTROL
        )

    def status(self) -> pb.StatusReply:
        return self.stub.Status(pb.StatusRequest(), timeout=TIMEOUT_CONTROL)

    def reset(self) -> pb.StatusReply:
        return self.stub.Reset(pb.ResetRequest(), timeout=TIMEOUT_CONTROL)

    def transcript(self) -> Transcript:
        return Transcript(
            self.stub.Transcript(pb.TranscriptRequest(), timeout=TIMEOUT_CONTROL)
        )


def parse_imposter_endpoints(line: str | bytes) -> ImposterEndpoints:
    if (endpoints := tp.endpoints_from_json_line(line)) is not None:
        return endpoints

    raise ImposterStartupError(f"could not parse imposter endpoints: {line!r}")


def read_imposter_endpoints(process: subprocess.Popen[bytes]) -> ImposterEndpoints:
    assert process.stdout is not None
    if not select.select([process.stdout], [], [], TIMEOUT_STARTUP)[0]:
        raise ImposterStartupError(
            f"Failed to read endpoints from stdout within {TIMEOUT_STARTUP}s"
        )

    line = process.stdout.readline()
    if not line:
        return_code = process.wait(timeout=TIMEOUT_SHUTDOWN)
        raise ImposterStartupError(f"imposter exited with code {return_code}")

    return parse_imposter_endpoints(line)


def stop_imposter(process: subprocess.Popen[bytes]) -> None:
    process.terminate()

    try:
        process.wait(timeout=TIMEOUT_SHUTDOWN)
    except subprocess.TimeoutExpired as e:
        process.kill()
        process.wait()
        raise ImposterShutdownError(
            f"Failed to terminate imposter within {TIMEOUT_SHUTDOWN}s"
        ) from e


@contextmanager
def run_imposter() -> Generator[ImposterEndpoints]:
    with tempfile.TemporaryDirectory(
        dir=tp.CONTROL_SOCK_DIR_PARENT, ignore_cleanup_errors=True
    ) as socket_dir:
        command = (
            sys.executable,
            "-m",
            IMPOSTER_MODULE,
            "--socket",
            str(Path(socket_dir) / tp.CONTROL_SOCK_NAME),
        )

        with subprocess.Popen(command, stdout=subprocess.PIPE) as process:
            try:
                yield read_imposter_endpoints(process)
            except BaseException:
                with suppress(ImposterShutdownError):
                    stop_imposter(process)
                raise

            stop_imposter(process)


@contextmanager
def connect_imposter(endpoints: ImposterEndpoints) -> Generator[AmqpImposter]:
    with grpc.insecure_channel(
        tp.uds_target(endpoints.socket_path), options=tp.GRPC_CHANNEL_OPTS
    ) as grpc_channel:
        yield AmqpImposter(endpoints, pb_grpc.ImposterStub(grpc_channel))


@contextmanager
def amqp_imposter(
    config: pb.SessionConfig | None = None,
) -> Generator[AmqpImposter]:
    with run_imposter() as endpoints, connect_imposter(endpoints) as imposter:
        if config is not None:
            imposter.configure(config)

        yield imposter
