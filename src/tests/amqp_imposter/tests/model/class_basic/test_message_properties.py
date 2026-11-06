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

from datetime import UTC, datetime
from decimal import Decimal

import pytest

from amqp_imposter.model import Method, Unrepresentable
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m
from amqp_imposter.model.constants import (
    DELIVERY_MODES,
    OCTET_MAX,
    PRIORITY_MAX,
    PRIORITY_MIN,
)

from ..helpers import (
    EXCHANGE,
    ROUTING_KEY,
    channelled,
    lookup_unique_sendable,
    receive_unwrap,
)

LEGAL_PRIORITIES = tuple(range(PRIORITY_MIN, PRIORITY_MAX + 1))
ILLEGAL_PRIORITIES = (PRIORITY_MIN - 1, PRIORITY_MAX + 1, OCTET_MAX)
ILLEGAL_DELIVERY_MODES = (min(DELIVERY_MODES) - 1, max(DELIVERY_MODES) + 1)

EVERY_VALUE = mf.Table(
    {
        "void": mf.Void(),
        "bit": True,
        "int": 42,
        "string": "text",
        "bytes": b"\x00\xff",
        "decimal": Decimal("1.25"),
        "timestamp": datetime(2026, 8, 22, tzinfo=UTC),
        "array": (1, "two", mf.Void()),
        "table": mf.Table({"nested": mf.Table({"deep": 1})}),
    }
)


def test_headers_are_returned_intact() -> None:
    server = channelled()

    receive_unwrap(
        server,
        m.BasicPublish(
            exchange=EXCHANGE,
            routing_key=ROUTING_KEY,
            mandatory=True,
            properties=mf.MessageProperties(headers=EVERY_VALUE),
        ),
    )

    returned = lookup_unique_sendable(server, m.BasicReturn)

    assert isinstance(returned.properties, mf.MessageProperties)
    assert returned.properties.headers == EVERY_VALUE


def publish_properties(properties: mf.MessageProperties) -> Method | Unrepresentable:
    return channelled().receive(
        m.BasicPublish(
            exchange=EXCHANGE, routing_key=ROUTING_KEY, properties=properties
        )
    )


@pytest.mark.parametrize("priority", ILLEGAL_PRIORITIES)
def test_bad_priority(priority: int) -> None:
    assert isinstance(
        publish_properties(mf.MessageProperties(priority=priority)), Unrepresentable
    )


@pytest.mark.parametrize("priority", LEGAL_PRIORITIES)
def test_good_priority(priority: int) -> None:
    assert not isinstance(
        publish_properties(mf.MessageProperties(priority=priority)), Unrepresentable
    )


@pytest.mark.parametrize("delivery_mode", ILLEGAL_DELIVERY_MODES)
def test_bad_delivery_mode(delivery_mode: int) -> None:
    assert isinstance(
        publish_properties(mf.MessageProperties(delivery_mode=delivery_mode)),
        Unrepresentable,
    )


@pytest.mark.parametrize("delivery_mode", DELIVERY_MODES)
def test_good_delivery_mode(delivery_mode: int) -> None:
    assert not isinstance(
        publish_properties(mf.MessageProperties(delivery_mode=delivery_mode)),
        Unrepresentable,
    )
