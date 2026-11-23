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

from amqp_imposter.model import domains as d
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m
from amqp_imposter.model.constants import (
    DELIVERY_MODES,
    PRIORITY_MAX,
    PRIORITY_MIN,
)

from ..helpers import (
    EXCHANGE,
    ROUTING_KEY,
    ErrorReply,
    channel_errors,
    channelled,
    error_replies_to,
    lookup_unique_sendable,
    receive_unwrap,
)

LEGAL_PRIORITIES = tuple(range(PRIORITY_MIN, PRIORITY_MAX + 1))
ILLEGAL_PRIORITIES = (PRIORITY_MAX + 1, d.Octet.MAX)
ILLEGAL_DELIVERY_MODES = (min(DELIVERY_MODES) - 1, max(DELIVERY_MODES) + 1)

EVERY_VALUE = mf.Table(
    {
        d.ShortStr("void"): mf.Void(),
        d.ShortStr("bit"): True,
        d.ShortStr("int"): d.SignedLongLong(42),
        d.ShortStr("string"): "text",
        d.ShortStr("bytes"): b"\x00\xff",
        d.ShortStr("decimal"): d.DecimalValue("1.25"),
        d.ShortStr("timestamp"): d.Timestamp(1787356800),
        d.ShortStr("array"): (d.SignedLongLong(1), "two", mf.Void()),
        d.ShortStr("table"): mf.Table(
            {d.ShortStr("nested"): mf.Table({d.ShortStr("deep"): d.SignedLongLong(1)})}
        ),
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


def publish_errors(properties: mf.MessageProperties) -> set[ErrorReply]:
    server = channelled()
    receive_unwrap(
        server,
        m.BasicPublish(
            exchange=EXCHANGE,
            routing_key=ROUTING_KEY,
            properties=properties,
        ),
    )
    return error_replies_to(server, m.BasicPublish)


@pytest.mark.parametrize("priority", ILLEGAL_PRIORITIES)
def test_bad_priority(priority: int) -> None:
    assert publish_errors(
        mf.MessageProperties(priority=d.Octet(priority))
    ) == channel_errors(mf.ReplyCode.PRECONDITION_FAILED)


@pytest.mark.parametrize("priority", LEGAL_PRIORITIES)
def test_good_priority(priority: int) -> None:
    assert not channel_errors(mf.ReplyCode.PRECONDITION_FAILED) & publish_errors(
        mf.MessageProperties(priority=d.Octet(priority))
    )


@pytest.mark.parametrize("delivery_mode", ILLEGAL_DELIVERY_MODES)
def test_bad_delivery_mode(delivery_mode: int) -> None:
    assert publish_errors(
        mf.MessageProperties(delivery_mode=d.Octet(delivery_mode))
    ) == channel_errors(mf.ReplyCode.PRECONDITION_FAILED)


@pytest.mark.parametrize("delivery_mode", DELIVERY_MODES)
def test_good_delivery_mode(delivery_mode: int) -> None:
    assert not channel_errors(mf.ReplyCode.PRECONDITION_FAILED) & publish_errors(
        mf.MessageProperties(delivery_mode=d.Octet(delivery_mode))
    )
