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
from typing import Any

import pika.spec as ps
import pytest

from amqp_imposter.amqp import method_framing as fr
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m
from tests.amqp.helpers import CONNECTION_START

from ..helpers import concrete

SEED = 7
FRAME_MAX = 131072
FRAME_MIN = 4096

B_HEARTBEAT = b"\x08\x00\x00\x00\x00\x00\x00\xce"

ACK = fr.MethodOnChannel(1, m.BasicAck(delivery_tag=1, multiple=False))
B_ACK = b"\x01\x00\x01\x00\x00\x00\r\x00<\x00P\x00\x00\x00\x00\x00\x00\x00\x01\x00\xce"

PUBLISH = fr.MethodOnChannel(
    1,
    m.BasicPublish(
        exchange="ex",
        routing_key="rk",
        mandatory=False,
        immediate=False,
        properties=mf.MessageProperties(content_type="text/plain", delivery_mode=2),
        body=b"hello",
    ),
)
B_PUBLISH = (
    b"\x01\x00\x01\x00\x00\x00\r\x00<\x00(\x00\x00\x02ex\x02rk\x00\xce"
    b"\x02\x00\x01\x00\x00\x00\x1a\x00<\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x90\x00\ntext/plain\x02\xce"
    b"\x03\x00\x01\x00\x00\x00\x05hello\xce"
)


def test_decode_protocol_header() -> None:
    assert fr.decode(fr.PROTOCOL_HEADER_091, FRAME_MAX) == (
        (fr.ProtocolHeader(fr.PROTOCOL_HEADER_091),),
        b"",
    )


def test_heartbeat_compose_decode_encode() -> None:
    assert fr.encode(fr.Heartbeat(), FRAME_MAX) == B_HEARTBEAT
    assert fr.decode(B_HEARTBEAT, FRAME_MAX) == ((fr.Heartbeat(),), b"")


@pytest.mark.parametrize(
    ("egress", "encoded"),
    (
        (ACK, B_ACK),
        (PUBLISH, B_PUBLISH),
    ),
    ids=("ack", "publish"),
)
def test_encodings(egress: fr.MethodOnChannel, encoded: bytes) -> None:
    assert fr.encode(egress, FRAME_MAX) == encoded
    assert fr.decode(encoded, FRAME_MAX) == ((egress,), b"")


# fmt: off
@pytest.mark.parametrize(
    "egress",
    (
        fr.MethodOnChannel(1, m.BasicAck(delivery_tag=7, multiple=True)),
        fr.MethodOnChannel(1, m.ChannelOpenOk()),
        fr.MethodOnChannel(0, m.ConnectionStart(version_major=0, version_minor=9, mechanisms=("PLAIN", "AMQPLAIN"), locales=("en_US",), publisher_confirms=True, basic_nack=True, consumer_cancel_notify=True,)),
        fr.MethodOnChannel(1, m.QueueDeclare(queue="q", passive=False, durable=True, exclusive=False, auto_delete=False, no_wait=False, arguments=mf.Table({"x-max-length": 10}))),
        fr.MethodOnChannel(1, m.BasicDeliver(consumer_tag="ct", delivery_tag=1, redelivered=False, exchange="ex", routing_key="rk", properties=mf.MessageProperties(content_type="text/plain"), body=b"hi")),
    ),
)
def test_compose_decode_encode(egress: fr.MethodOnChannel) -> None:
    assert fr.decode(fr.encode(egress, FRAME_MAX), FRAME_MAX) == ((egress,), b"")

# fmt: on


def test_decode_in_increments() -> None:
    other = fr.MethodOnChannel(1, m.BasicAck(delivery_tag=2, multiple=False))
    buffer = fr.encode(ACK, FRAME_MAX) + fr.encode(other, FRAME_MAX)

    initial_items, initial_carry = fr.decode(buffer, FRAME_MAX)
    assert initial_items == (ACK, other)
    assert initial_carry == b""

    seen_items: list[fr.Ingress] = []
    carry = b""
    for byte in buffer:
        items, carry = fr.decode(carry + bytes([byte]), FRAME_MAX)
        seen_items.extend(items)

    assert tuple(seen_items) == initial_items
    assert carry == b""


def test_frame_remainder() -> None:
    encoded = fr.encode(PUBLISH, FRAME_MAX)
    i_cut = len(encoded) - 3

    items, rest = fr.decode(encoded[:i_cut], FRAME_MAX)
    assert items == ()
    assert rest == encoded[:i_cut]

    assert fr.decode(rest + encoded[i_cut:], FRAME_MAX) == ((PUBLISH,), b"")


def test_frame_body_remainder() -> None:
    big = fr.MethodOnChannel(
        1,
        m.BasicDeliver(
            consumer_tag="c",
            delivery_tag=1,
            redelivered=False,
            exchange="e",
            routing_key="r",
            properties=mf.MessageProperties(),
            body=b"x" * 5000,
        ),
    )
    assert fr.decode(fr.encode(big, FRAME_MIN), FRAME_MAX) == ((big,), b"")


@pytest.mark.parametrize(
    "bad",
    (
        pytest.param(B_ACK[:-1] + b"\x00", id="bad_frame_end"),
        pytest.param(b"\x63\x00\x01\x00\x00\x00\x01\x00\xce", id="bad_frame_type"),
        pytest.param(
            b"\x01\x00\x01\x00\x00\x00\x04\x00\x3c\x03\xe7\xce",
            id="unknown_method",
        ),
        pytest.param(
            B_PUBLISH.replace(b"text/plain\x02\xce", b"text/plain\x02\x00"),
            id="bad_header_frame_end",
        ),
    ),
)
def test_bad_frames(bad: bytes) -> None:
    items, rest = fr.decode(bad, FRAME_MAX)
    assert [type(one) for one in items] == [fr.Undecodable]
    assert rest == b""


def test_buffer_maxed() -> None:
    huge = b"\x01\x00\x01" + (10_000_000).to_bytes(4, "big") + b"\x00" * 200

    items, rest = fr.decode(huge, 100)
    assert [type(one) for one in items] == [fr.Undecodable]
    assert rest == b""


EMPTY_ON_THE_WIRE: dict[str, Any] = {
    "arguments": mf.Table(),
    "properties": mf.MessageProperties(),
}


def as_received(method: m.Method) -> m.Method:
    return m.set_fields(
        method,
        {
            name: EMPTY_ON_THE_WIRE[name]
            if value is None and name in EMPTY_ON_THE_WIRE
            else value
            for name, value in m.get_fields(method).items()
        },
    )


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_every_kind_compose_decode_encode(kind: type[m.Method]) -> None:
    egress = fr.MethodOnChannel(1, concrete(kind, SEED))
    received = fr.MethodOnChannel(egress.channel, as_received(egress.method))

    assert fr.decode(fr.encode(egress, FRAME_MAX), FRAME_MAX) == ((received,), b"")


CONNECTION_START_CAPABILITIES_OFF = replace(
    CONNECTION_START,
    publisher_confirms=False,
    basic_nack=False,
    consumer_cancel_notify=False,
)


def start_spec(capabilities: Any) -> Any:
    spec = ps.Connection.Start(server_properties={fr.CAPABILITIES: capabilities})
    spec.mechanisms, spec.locales = "PLAIN", "en_US"

    return spec


@pytest.mark.parametrize("capabilities", ("nonsense", 7, [], None), ids=repr)
def test_unusable_capabilities_are_absent(capabilities: Any) -> None:
    assert fr.from_pika(start_spec(capabilities)) == CONNECTION_START_CAPABILITIES_OFF


def test_capabilities_are_read() -> None:
    method = fr.from_pika(start_spec({fr.BASIC_NACK: True}))

    assert method == replace(CONNECTION_START_CAPABILITIES_OFF, basic_nack=True)


def test_absent_longstr_is_empty() -> None:
    assert fr.from_pika(ps.Connection.StartOk()) == m.ConnectionStartOk(
        mechanism="PLAIN", locale="en_US", response=""
    )


def test_absent_delimited_longstr_is_empty() -> None:
    spec = ps.Connection.Start()
    spec.mechanisms, spec.locales = "", ""

    assert fr.from_pika(spec) == replace(
        CONNECTION_START_CAPABILITIES_OFF, mechanisms=(), locales=()
    )
