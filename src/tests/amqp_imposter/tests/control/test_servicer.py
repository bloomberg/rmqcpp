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
from dataclasses import replace

import pytest

from amqp_imposter.amqp.server import HOST
from amqp_imposter.amqp.session import DEFAULT_SESSION_CONFIG
from amqp_imposter.amqp.transcript import TranscriptEvent as ev
from amqp_imposter.control import config_proto as cp
from amqp_imposter.control import service as sv
from amqp_imposter.control.v1 import imposter_pb2 as pb
from amqp_imposter.imposter import Imposter, ImposterError
from tests.amqp.helpers import poll_until, transcript_events

from .helpers import make_imposter

WHEN = 2003


def test_status_output() -> None:
    imposter = make_imposter()

    reply = sv.status_reply(imposter, epoch_micros=WHEN)

    assert not reply.HasField("amqp_port")
    assert reply.amqp_greeted is False
    assert reply.epoch_micros == WHEN
    assert reply.transcript_lines == 1
    assert reply.config == cp.to_proto(DEFAULT_SESSION_CONFIG)


async def test_status_echoes_config() -> None:
    imposter = make_imposter()
    await sv.configure(imposter, pb.SessionConfig(seed=7))

    reply = sv.status_reply(imposter, epoch_micros=WHEN)

    assert cp.from_proto(reply.config) == imposter.session.config


async def test_configure_applies() -> None:
    imposter = make_imposter()

    await sv.configure(imposter, pb.SessionConfig(seed=9))

    assert imposter.session.config == replace(DEFAULT_SESSION_CONFIG, seed=9)


async def test_configure_resets() -> None:
    imposter = make_imposter()
    await sv.configure(imposter, pb.SessionConfig(seed=7, idle_millis=7))
    imposter.session.on_connected()

    await sv.configure(imposter, pb.SessionConfig(seed=9))

    assert imposter.session.config == replace(DEFAULT_SESSION_CONFIG, seed=9)
    assert sv.status_reply(imposter, WHEN).transcript_lines == 1


async def test_empty_configure_is_reset() -> None:
    imposter = make_imposter()
    await sv.configure(imposter, pb.SessionConfig(seed=7))

    await sv.configure(imposter, pb.SessionConfig())

    assert imposter.session.config == DEFAULT_SESSION_CONFIG


async def test_bad_configure_noop() -> None:
    imposter = make_imposter()
    await sv.configure(imposter, pb.SessionConfig(seed=7))
    imposter.session.on_connected()
    config = imposter.session.config
    lines = sv.status_reply(imposter, WHEN).transcript_lines

    with pytest.raises(ImposterError):
        await sv.configure(imposter, pb.SessionConfig(seed=9, stop_weight=0))

    assert imposter.session.config == config
    assert sv.status_reply(imposter, WHEN).transcript_lines == lines


def test_transcript_line_count() -> None:
    imposter = make_imposter()
    imposter.session.on_connected()

    reply = sv.status_reply(imposter, WHEN)

    assert reply.transcript_lines == len(imposter.session.transcript())


async def test_line_count_includes_dropped() -> None:
    imposter = make_imposter()
    await sv.configure(imposter, pb.SessionConfig(max_transcript_lines=2))
    imposter.session.on_connected()
    imposter.session.on_disconnected()

    reply = sv.status_reply(imposter, WHEN)

    assert reply.transcript_lines > len(imposter.session.transcript())


async def connect_client(
    imposter: Imposter,
) -> tuple[asyncio.StreamReader, asyncio.StreamWriter]:
    port = await imposter.amqp_server.start()
    client = await asyncio.wait_for(asyncio.open_connection(HOST, port), 2.0)
    await poll_until(lambda: imposter.amqp_server.writer is not None)

    return client


async def test_configure_drops_connected_client() -> None:
    imposter = make_imposter()
    reader, writer = await connect_client(imposter)

    await sv.configure(imposter, pb.SessionConfig(seed=9))

    assert await asyncio.wait_for(reader.read(), 2.0) == b""
    assert transcript_events(imposter.session.transcript()) == (ev.SESSION_STARTED,)

    writer.close()
    await imposter.amqp_server.stop()


async def test_bad_configure_keeps_connected_client() -> None:
    imposter = make_imposter()
    _, writer = await connect_client(imposter)

    with pytest.raises(ImposterError):
        await sv.configure(imposter, pb.SessionConfig(seed=9, stop_weight=0))

    assert imposter.amqp_server.writer is not None
    assert ev.TCP_DOWN not in transcript_events(imposter.session.transcript())

    writer.close()
    await imposter.amqp_server.stop()
