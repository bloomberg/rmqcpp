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

from typing import Any

from amqp_imposter.model import domains as d
from amqp_imposter.model import generators as g
from amqp_imposter.model import method_unification as mu
from amqp_imposter.model import methods as m

EXACT_VALUES: dict[str, Any] = {
    "version_major": d.Octet(0),
    "version_minor": d.Octet(9),
    "mechanisms": ("PLAIN", "AMQPLAIN"),
    "locales": ("en_US",),
    "publisher_confirms": True,
    "basic_nack": True,
    "consumer_cancel_notify": True,
    "mechanism": d.ShortStr("PLAIN"),
    "locale": d.ShortStr("en_US"),
    "channel_max": d.Short(2047),
    "frame_max": d.Long(131072),
    "heartbeat": d.Short(60),
    "reply_code": d.Short(406),
    "reply_text": d.ShortStr("FORCED"),
    "class_id": d.Short(60),
    "method_id": d.Short(40),
    "consumer_tag": d.ShortStr("ctag"),
    "delivery_tag": d.LongLong(7),
    "exchange": d.ShortStr("ex"),
    "routing_key": d.ShortStr("rk"),
}


def filled[M: m.Method](kind: type[M]) -> M:
    return kind(**{name: EXACT_VALUES[name] for name in m.exact_fields(kind)})


def concrete[M: m.Method](kind: type[M], seed: int) -> M:
    method, _ = mu.concretise(filled(kind), g.GenState(seed))

    return method
