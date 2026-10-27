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

import pytest

from amqp_imposter.model import Hole, ModelServer, Unrepresentable
from amqp_imposter.model import methods as m

from ..helpers import (
    GREETED_STATES,
    MakeServer,
    channelled,
    receive_unwrap,
    starting,
)


def test_nothing_sendable_before_the_header() -> None:
    assert ModelServer().sendables() == ()


@pytest.mark.parametrize("make_server", GREETED_STATES, ids=lambda b: b.__name__)
def test_deterministic(make_server: MakeServer) -> None:
    server = make_server()

    assert server.sendables() == server.sendables()


@pytest.mark.parametrize("make_server", GREETED_STATES, ids=lambda b: b.__name__)
def test_no_holes(make_server: MakeServer) -> None:
    for method in make_server().sendables():
        assert Hole() not in m.get_fields(method).values(), method


@pytest.mark.parametrize("make_server", GREETED_STATES, ids=lambda b: b.__name__)
def test_every_enumerated_method_is_sendable(make_server: MakeServer) -> None:
    for method in make_server().sendables():
        assert not isinstance(make_server().send(method), Unrepresentable)


def test_unenumerated_method_is_illegal_send() -> None:
    server = starting()

    assert isinstance(server.send(m.ConnectionOpenOk()), Unrepresentable)


def test_send_is_destructive() -> None:
    server = channelled()

    receive_unwrap(server, m.ConfirmSelect())
    select_ok = m.ConfirmSelectOk()
    server.send(select_ok)

    assert select_ok not in server.sendables()
