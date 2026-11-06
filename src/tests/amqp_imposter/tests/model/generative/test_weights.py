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

from amqp_imposter.model import BASE_WEIGHT, WeightedPattern
from amqp_imposter.model import method_weights as w
from amqp_imposter.model import methods as m
from tests.model.helpers import CONSUMER_TAG


def test_init_with_default_weight() -> None:
    assert WeightedPattern(m.BasicAck()).weight == BASE_WEIGHT


def test_exact_field_is_matchable() -> None:
    WeightedPattern(m.BasicAck(delivery_tag=2), 0)
    WeightedPattern(m.BasicCancel(consumer_tag=CONSUMER_TAG), 8)
    WeightedPattern(w.UNPROMPTED_CONNECTION_CLOSE, 0)
    WeightedPattern(m.ChannelClose(reply_text="NOT_FOUND"), 0)
    WeightedPattern(m.ConnectionClose(reply_text="NOT_ALLOWED"), 0)


def test_arbitrary_field_is_not_matchable() -> None:
    with pytest.raises(m.BadPattern):
        WeightedPattern(m.BasicNack(requeue=True))

    with pytest.raises(m.BadPattern):
        WeightedPattern(m.BasicDeliver(redelivered=True))


def test_bad_weight() -> None:
    with pytest.raises(w.BadWeight):
        WeightedPattern(m.BasicAck(), -1)


def test_unprompted_teardown_implicitly_disabled() -> None:
    weighted_patterns = w.implicit_weighted_patterns(
        (m.BasicAck, m.BasicCancel, m.ConnectionClose)
    )
    zeros = {
        c.pattern for c in weighted_patterns if c.weight == w.UNPROMPTED_TEARDOWN_WEIGHT
    }

    assert {
        w.UNPROMPTED_CONNECTION_CLOSE,
        w.UNPROMPTED_CHANNEL_CLOSE,
        w.UNPROMPTED_CANCEL,
    } <= zeros
    assert all(
        wp.weight == BASE_WEIGHT for wp in weighted_patterns if wp.pattern not in zeros
    )
