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

from amqp_imposter.model import Unrepresentable
from amqp_imposter.model import methods as m

from ..helpers import channelled, connected, receive_unwrap


def test_illegal_receive_does_not_advance() -> None:
    server = channelled()
    before_sendable = server.sendables()
    before_gen = server.gen_state

    assert isinstance(server.receive(m.ConnectionTuneOk(0, 0, 0)), Unrepresentable)

    assert server.sendables() == before_sendable
    assert server.gen_state == before_gen


def test_fully_specified_receive_splits_gen() -> None:
    server = connected()
    before = server.gen_state

    receive_unwrap(server, m.ChannelOpen())

    assert server.gen_state != before
