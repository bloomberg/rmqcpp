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

from dataclasses import fields
from datetime import UTC, datetime
from decimal import Decimal

import pytest

from amqp_imposter.amqp import transcript as tr
from amqp_imposter.control import method_proto as mp
from amqp_imposter.control.v1 import imposter_pb2 as pb
from amqp_imposter.imposter import ImposterError
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import concrete

SAMPLES = 10


NESTED_TABLE = mf.Table({"inner": 1, "deeper": mf.Table({"leaf": "x"})})

TABLE_VALUES: tuple[mf.TableValue, ...] = (
    True,
    False,
    0,
    -1,
    2**40,
    "",
    "text",
    b"",
    b"\xde\xad\xbe\xef",
    Decimal("1.25"),
    Decimal("-0.001"),
    datetime(2026, 9, 6, 12, 30, 45, 123456, tzinfo=UTC),
    mf.Void(),
    NESTED_TABLE,
    (1, "two", False),
    (mf.Void(), NESTED_TABLE),
    (),
)

PROPERTIES: tuple[mf.MessageProperties, ...] = (
    mf.MessageProperties(),
    mf.MessageProperties(content_type="text/plain"),
    mf.MessageProperties(priority=0, delivery_mode=2, timestamp=0),
    mf.MessageProperties(headers=mf.Table()),
    mf.MessageProperties(headers=NESTED_TABLE, app_id="", user_id="u"),
)


@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_all_methods_total(kind: type[m.Method]) -> None:
    [descriptor] = [
        field.message_type
        for field in pb.Method.DESCRIPTOR.fields
        if field.message_type and field.message_type.name == kind.__name__
    ]

    proto_fields = {field.name for field in descriptor.fields}
    real_fields = {field.name for field in fields(kind)}

    assert proto_fields == real_fields


@pytest.mark.parametrize("seed", range(SAMPLES))
@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_concrete_compose_from_to(kind: type[m.Method], seed: int) -> None:
    method = concrete(kind, seed)

    assert mp.from_proto(mp.to_proto(method)) == method


@pytest.mark.parametrize("seed", range(SAMPLES))
@pytest.mark.parametrize("kind", m.METHOD_KINDS, ids=lambda k: k.__name__)
def test_concrete_compose_from_serialise_to(seed: int, kind: type[m.Method]) -> None:
    method = concrete(kind, seed)
    encoded = mp.to_proto(method).SerializeToString()

    assert mp.from_proto(pb.Method.FromString(encoded)) == method


def test_empty_method() -> None:
    with pytest.raises(ImposterError):
        mp.from_proto(pb.Method())


def test_empty_value() -> None:
    with pytest.raises(ImposterError):
        mp.value_from_proto(pb.AmqpValue())


@pytest.mark.parametrize("value", TABLE_VALUES, ids=repr)
def test_table_values_compose_from_To(value: mf.TableValue) -> None:
    assert mp.value_from_proto(mp.value_to_proto(value)) == value


@pytest.mark.parametrize("value", TABLE_VALUES, ids=repr)
def test_table_value_types_compose_from_to(value: mf.TableValue) -> None:
    decoded = mp.value_from_proto(mp.value_to_proto(value))

    assert type(decoded) is type(value)


def test_table_compose_from_to() -> None:
    table = mf.Table(
        {
            "bool": True,
            "long": 7,
            "string": "s",
            "bytes": b"\x00\xff",
            "decimal": Decimal("3.5"),
            "timestamp": datetime(2026, 1, 1, tzinfo=UTC),
            "void": mf.Void(),
            "table": NESTED_TABLE,
            "array": (1, mf.Void()),
        }
    )
    assert mp.table_from_proto(mp.table_to_proto(table)) == table


def test_empty_table_compose_from_to() -> None:
    assert mp.table_from_proto(mp.table_to_proto(mf.Table())) == mf.Table()


@pytest.mark.parametrize("properties", PROPERTIES, ids=range(len(PROPERTIES)))
def test_properties_compose_from_to(properties: mf.MessageProperties) -> None:
    assert mp.properties_from_proto(mp.properties_to_proto(properties)) == properties


def test_empty_properties_vs_absent() -> None:
    empty = m.BasicDeliver(
        consumer_tag="c", delivery_tag=1, properties=mf.MessageProperties()
    )
    absent = m.BasicDeliver(consumer_tag="c", delivery_tag=1, properties=None)

    assert mp.from_proto(mp.to_proto(empty)) != mp.from_proto(mp.to_proto(absent))


def test_empty_arguments_vs_absent() -> None:
    empty = m.QueueDeclare(queue="q", arguments=mf.Table())
    absent = m.QueueDeclare(queue="q", arguments=None)

    assert mp.from_proto(mp.to_proto(empty)) != mp.from_proto(mp.to_proto(absent))


def test_line_without_method() -> None:
    line = tr.TranscriptLine(
        index=3,
        micros=7,
        epoch_micros=1732,
        event=tr.TranscriptEvent.PROTOCOL_HEADER,
        wire_bytes=b"AMQP\x00\x00\x09\x01",
    )
    message = mp.line_to_proto(line)

    assert not message.HasField("method")
    assert (message.index, message.micros, message.epoch_micros) == (3, 7, 1732)
    assert message.wire_bytes == b"AMQP\x00\x00\x09\x01"
    assert message.detail == ""


def test_line_with_method() -> None:
    method = m.BasicDeliver(consumer_tag="ct", delivery_tag=1)
    line = tr.TranscriptLine(
        index=6,
        micros=10,
        epoch_micros=1735,
        event=tr.TranscriptEvent.METHOD_EGRESS,
        method=method,
        wire_bytes=b"\x01\x02\x03",
    )
    message = mp.line_to_proto(line)

    assert message.method == mp.to_proto(method)


def test_line_detail() -> None:
    line = tr.TranscriptLine(
        index=4,
        micros=8,
        epoch_micros=1733,
        event=tr.TranscriptEvent.FRAME_MALFORMED,
        detail="short frame",
        wire_bytes=b"\xff\xff",
    )
    assert mp.line_to_proto(line).detail == "short frame"
