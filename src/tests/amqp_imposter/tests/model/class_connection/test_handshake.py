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

from dataclasses import replace

import pytest

from amqp_imposter.model import (
    NegotiationOptions,
    Unrepresentable,
    constants,
)
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    connection_errors,
    error_replies_to,
    greeted,
    lookup_unique_sendable,
    opening,
    receive_unwrap,
    send_unwrap,
    start_ok_of,
    subsets_any_sendable,
    tune_ok_of,
    tuned,
    tuning,
)

NEGOTIATION_OPTIONS = NegotiationOptions(
    mechanisms=("EXTERNAL", "PLAIN"),
    locales=("en_GB",),
    publisher_confirms=False,
    basic_nack=False,
    consumer_cancel_notify=False,
    channel_max=7,
    frame_max=constants.FRAME_MIN_FLOOR,
    heartbeat=60,
)


def test_start_with_options() -> None:
    start = lookup_unique_sendable(greeted(NEGOTIATION_OPTIONS), m.ConnectionStart)

    assert start == m.ConnectionStart(
        version_major=constants.PROTOCOL_MAJOR,
        version_minor=constants.PROTOCOL_MINOR,
        mechanisms=NEGOTIATION_OPTIONS.mechanisms,
        locales=NEGOTIATION_OPTIONS.locales,
        publisher_confirms=NEGOTIATION_OPTIONS.publisher_confirms,
        basic_nack=NEGOTIATION_OPTIONS.basic_nack,
        consumer_cancel_notify=NEGOTIATION_OPTIONS.consumer_cancel_notify,
    )


def test_tune_with_options() -> None:
    tune = lookup_unique_sendable(tuning(NEGOTIATION_OPTIONS), m.ConnectionTune)

    assert tune == m.ConnectionTune(
        channel_max=NEGOTIATION_OPTIONS.channel_max,
        frame_max=NEGOTIATION_OPTIONS.frame_max,
        heartbeat=NEGOTIATION_OPTIONS.heartbeat,
    )


def test_start_ok_enables_tune() -> None:
    assert subsets_any_sendable(tuning(), m.ConnectionTune)


def test_tune_ok_enables_open() -> None:
    server = tuned(NEGOTIATION_OPTIONS)

    receive_unwrap(server, m.ConnectionOpen())

    assert subsets_any_sendable(server, m.ConnectionOpenOk)


def test_open_optional_errors() -> None:
    assert error_replies_to(opening(), m.ConnectionOpen) == connection_errors(
        mf.ReplyCode.INVALID_PATH, mf.ReplyCode.NOT_ALLOWED
    )


def test_open_error_closes_connection() -> None:
    server = opening()

    send_unwrap(server, m.ConnectionClose, reply_code=mf.ReplyCode.NOT_ALLOWED)

    assert not subsets_any_sendable(server, m.ConnectionOpenOk)

    receive_unwrap(server, m.ConnectionCloseOk())

    assert server.receive_protocol_header() is None


@pytest.mark.parametrize(
    "mechanism",
    ("whatever", ""),
)
def test_start_ok_contradicts_mechanism(mechanism: str) -> None:
    # TODO edit once meaningful negotiation is implemented
    server = greeted(NEGOTIATION_OPTIONS)
    start_ok = start_ok_of(send_unwrap(server, m.ConnectionStart))

    assert isinstance(
        server.receive(replace(start_ok, mechanism=mechanism)), Unrepresentable
    )


def test_start_ok_contradicts_locale() -> None:
    # TODO edit once meaningful negotiation is implemented, we should remove this
    server = greeted(NEGOTIATION_OPTIONS)
    start_ok = start_ok_of(send_unwrap(server, m.ConnectionStart))

    assert isinstance(
        server.receive(replace(start_ok, locale="fr_FR")), Unrepresentable
    )


@pytest.mark.parametrize(
    "alteration",
    (
        {"channel_max": NEGOTIATION_OPTIONS.channel_max + 1},
        {"frame_max": NEGOTIATION_OPTIONS.frame_max + 1},
        {"heartbeat": NEGOTIATION_OPTIONS.heartbeat + 1},
    ),
    ids=("channel_max", "frame_max", "heartbeat"),
)
def test_tune_ok_contradicts_tune(alteration: dict[str, int]) -> None:
    server = tuning(NEGOTIATION_OPTIONS)
    tune_ok = tune_ok_of(send_unwrap(server, m.ConnectionTune))

    assert isinstance(server.receive(replace(tune_ok, **alteration)), Unrepresentable)
