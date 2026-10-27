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

OCTET_MAX = 2**8 - 1
SHORT_MAX = 2**16 - 1
LONG_MAX = 2**32 - 1
LONGLONG_MAX = 2**64 - 1

PROTOCOL_MAJOR = 0
PROTOCOL_MINOR = 9
PROTOCOL_REVISION = 1

FRAME_END = 206
FRAME_OVERHEAD = 8
FRAME_MIN_FLOOR = 4096
CHANNEL_MAX_CEIL = SHORT_MAX
FRAME_MAX_CEIL = LONG_MAX
HEARTBEAT_MAX = SHORT_MAX

NAME_MAX = 127
MESSAGE_BODY_MAX = FRAME_MIN_FLOOR - FRAME_OVERHEAD
SHORTSTR_MAX = OCTET_MAX

PRIORITY_MIN = 0
PRIORITY_MAX = 9
DELIVERY_MODES = (1, 2)

DEFAULT_OVERFLOWS = ("drop-head", "reject-publish")
DEFAULT_QUEUE_TYPES = ("classic", "quorum")
DEFAULT_EXCHANGE_TYPES = ("direct", "fanout", "topic", "headers")
DEFAULT_CONTENT_TYPES = ("text/plain", "application/xml", "application/json")
DEFAULT_CONTENT_ENCODINGS = ("gzip", "zstd", "identity")

DEFAULT_MECHANISMS = ("PLAIN",)
DEFAULT_LOCALES = ("en_US",)
DEFAULT_PUBLISHER_CONFIRMS = True
DEFAULT_BASIC_NACK = True
DEFAULT_CONSUMER_CANCEL_NOTIFY = True
DEFAULT_CHANNEL_MAX = 2047
DEFAULT_FRAME_MAX = 131072
DEFAULT_HEARTBEAT = 0
DEFAULT_VHOST = "/"
