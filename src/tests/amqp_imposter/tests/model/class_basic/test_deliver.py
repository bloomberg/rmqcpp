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

from ..helpers import (
    CONSUMER_TAG,
    consuming,
    deliver,
)


def test_send_delivery_tag_too_high() -> None:
    server = consuming(prefetch_count=2)

    deliver(server, 1)

    assert isinstance(
        server.send(m.BasicDeliver(consumer_tag=CONSUMER_TAG, delivery_tag=3)),
        Unrepresentable,
    )
