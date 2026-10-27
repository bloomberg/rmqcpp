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

from pathlib import Path

from grpc import aio

from amqp_imposter.amqp.server import AmqpServer
from amqp_imposter.amqp.session import Session
from amqp_imposter.control import transport as tp
from amqp_imposter.imposter import Imposter

SHORT_TEMP_DIR = "/tmp"


def make_imposter() -> Imposter:
    session = Session()

    return Imposter(session=session, amqp_server=AmqpServer(session))


def channel_for(path: Path) -> aio.Channel:
    return aio.insecure_channel(tp.uds_target(path), options=tp.GRPC_CHANNEL_OPTS)
