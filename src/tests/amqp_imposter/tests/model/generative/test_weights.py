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

from amqp_imposter.model import BASE_WEIGHT, WeightedSelector
from amqp_imposter.model import domains as d
from amqp_imposter.model import method_weights as w
from amqp_imposter.model import methods as m
from tests.model.helpers import CONSUMER_TAG


def test_init_with_default_weight() -> None:
    assert WeightedSelector(m.BasicAck()).weight == BASE_WEIGHT


def test_exact_field_is_matchable() -> None:
    WeightedSelector(m.BasicAck(delivery_tag=d.LongLong(2)), 0)
    WeightedSelector(m.BasicCancel(consumer_tag=CONSUMER_TAG), 8)
    WeightedSelector(w.UNPROMPTED_CONNECTION_CLOSE, 0)
    WeightedSelector(m.ChannelClose(reply_text=d.ShortStr("NOT_FOUND")), 0)
    WeightedSelector(m.ConnectionClose(reply_text=d.ShortStr("NOT_ALLOWED")), 0)


def test_arbitrary_field_is_not_matchable() -> None:
    with pytest.raises(m.BadSelector):
        WeightedSelector(m.BasicNack(requeue=True))

    with pytest.raises(m.BadSelector):
        WeightedSelector(m.BasicDeliver(redelivered=True))


def test_bad_weight() -> None:
    with pytest.raises(w.BadWeight):
        WeightedSelector(m.BasicAck(), -1)


def test_unprompted_teardown_implicitly_disabled() -> None:
    weighted_selectors = w.implicit_weighted_selectors(
        (m.BasicAck, m.BasicCancel, m.ConnectionClose)
    )
    zeros = {
        c.selector
        for c in weighted_selectors
        if c.weight == w.UNPROMPTED_TEARDOWN_WEIGHT
    }

    assert {
        w.UNPROMPTED_CONNECTION_CLOSE,
        w.UNPROMPTED_CHANNEL_CLOSE,
        w.UNPROMPTED_CANCEL,
    } <= zeros
    assert all(
        ws.weight == BASE_WEIGHT
        for ws in weighted_selectors
        if ws.selector not in zeros
    )
