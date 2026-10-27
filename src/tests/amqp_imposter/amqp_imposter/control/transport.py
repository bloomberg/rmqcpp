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

import json
from contextlib import suppress
from dataclasses import dataclass
from pathlib import Path

UDS_SCHEME = "unix://"
CONTROL_SOCK_NAME = "control.sock"
CONTROL_SOCK_DIR_PARENT = Path("/tmp")

GRPC_MAX_MESSAGE_BYTES = 64 * 1024 * 1024  # 64 MiB
GRPC_CHANNEL_OPTS = (
    ("grpc.max_send_message_length", GRPC_MAX_MESSAGE_BYTES),
    ("grpc.max_receive_message_length", GRPC_MAX_MESSAGE_BYTES),
)


def uds_target(socket_path: Path) -> str:
    return f"{UDS_SCHEME}{socket_path}"


@dataclass(frozen=True, slots=True)
class ImposterEndpoints:
    socket_path: Path
    amqp_port: int

    def to_json_line(self) -> str:
        return (
            json.dumps({"socket": str(self.socket_path), "amqp_port": self.amqp_port})
            + "\n"
        )


def endpoints_from_json_line(line: str | bytes) -> ImposterEndpoints | None:
    with suppress(ValueError):
        match json.loads(line):
            case {"socket": str(socket_path), "amqp_port": int(amqp_port)}:
                return ImposterEndpoints(Path(socket_path), amqp_port)

    return None
