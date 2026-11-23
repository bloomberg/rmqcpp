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

from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import methods as m

from ..helpers import (
    channelled,
    lookup_sendables,
    lookup_unique_sendable,
    message_content,
    publish,
    send_unwrap,
    subsets_any_sendable,
)


def test_no_confirm_mandatory() -> None:
    server = channelled()

    published = publish(server, body=b"limestone", mandatory=True)

    assert not subsets_any_sendable(server, m.BasicAck)
    assert not subsets_any_sendable(server, m.BasicNack)

    unroutable = lookup_unique_sendable(server, m.BasicReturn)

    assert unroutable.reply_code == mf.ReplyCode.UNROUTABLE
    assert message_content(unroutable) == message_content(published)

    server.send(unroutable)

    assert not subsets_any_sendable(server, m.BasicReturn)
    assert not subsets_any_sendable(server, m.BasicAck)
    assert not subsets_any_sendable(server, m.BasicNack)


def test_no_confirm_no_mandatory() -> None:
    server = channelled()

    publish(server)

    assert not subsets_any_sendable(server, m.BasicAck)
    assert not subsets_any_sendable(server, m.BasicNack)
    assert not subsets_any_sendable(server, m.BasicReturn)


def test_no_confirm_after_return_sent() -> None:
    server = channelled()

    publish(server, mandatory=True)
    send_unwrap(server, m.BasicReturn)

    assert not subsets_any_sendable(server, m.BasicAck)


def test_each_mandatory_publish_returns() -> None:
    server = channelled()

    publish(server, body=b"one", mandatory=True)
    publish(server, body=b"two", mandatory=True)

    assert len(lookup_sendables(server, m.BasicReturn)) == 2

    server.send(lookup_unique_sendable(server, m.BasicReturn, body=b"one"))

    assert len(lookup_sendables(server, m.BasicReturn)) == 1
    assert lookup_unique_sendable(server, m.BasicReturn, body=b"two")
