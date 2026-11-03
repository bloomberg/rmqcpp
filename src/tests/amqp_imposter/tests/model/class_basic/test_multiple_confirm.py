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

from amqp_imposter.model import methods as m

from ..helpers import confirming, delivery_tags, publish, send_unwrap


def test_publish_multiple_ack() -> None:
    server = confirming()

    publish(server, body=b"one")
    publish(server, body=b"two")
    publish(server, body=b"three")

    assert delivery_tags(server, m.BasicAck) == {
        (1, False),
        (2, False),
        (3, False),
        (1, True),
        (2, True),
        (3, True),
    }

    send_unwrap(server, m.BasicAck, delivery_tag=2, multiple=True)

    assert delivery_tags(server, m.BasicAck) == {(3, False), (3, True)}
    assert delivery_tags(server, m.BasicNack) == {(3, False), (3, True)}

    send_unwrap(server, m.BasicAck, delivery_tag=3, multiple=True)

    assert delivery_tags(server, m.BasicAck) == set()
    assert delivery_tags(server, m.BasicNack) == set()


def test_publish_multiple_nack() -> None:
    server = confirming()

    publish(server, body=b"one")
    publish(server, body=b"two")
    publish(server, body=b"three")

    send_unwrap(server, m.BasicNack, delivery_tag=2, multiple=True)

    assert delivery_tags(server, m.BasicNack) == {(3, False), (3, True)}
    assert delivery_tags(server, m.BasicAck) == {(3, False), (3, True)}
