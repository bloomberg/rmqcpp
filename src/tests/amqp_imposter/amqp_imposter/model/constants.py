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

from .domains import Long, Octet, Short, ShortStr

PROTOCOL_MAJOR = Octet(0)
PROTOCOL_MINOR = Octet(9)
PROTOCOL_REVISION = 1

FRAME_END = 206
FRAME_OVERHEAD = 8
FRAME_MIN_FLOOR = 4096

NAME_MAX = 127
MESSAGE_BODY_MAX = FRAME_MIN_FLOOR - FRAME_OVERHEAD

PRIORITY_MIN = 0
PRIORITY_MAX = 9
DELIVERY_MODES = (Octet(1), Octet(2))

DEFAULT_OVERFLOWS = ("drop-head", "reject-publish")
DEFAULT_QUEUE_TYPES = ("classic", "quorum")
DEFAULT_EXCHANGE_TYPES = (
    ShortStr("direct"),
    ShortStr("fanout"),
    ShortStr("topic"),
    ShortStr("headers"),
)
DEFAULT_CONTENT_TYPES = (
    ShortStr("text/plain"),
    ShortStr("application/xml"),
    ShortStr("application/json"),
)
DEFAULT_CONTENT_ENCODINGS = (ShortStr("gzip"), ShortStr("zstd"), ShortStr("identity"))

DEFAULT_MECHANISMS = ("PLAIN",)
DEFAULT_LOCALES = ("en_US",)
DEFAULT_PUBLISHER_CONFIRMS = True
DEFAULT_BASIC_NACK = True
DEFAULT_CONSUMER_CANCEL_NOTIFY = True
DEFAULT_CHANNEL_MAX = Short(2047)
DEFAULT_FRAME_MAX = Long(131072)
DEFAULT_HEARTBEAT = Short(0)
DEFAULT_VHOST = ShortStr("/")
