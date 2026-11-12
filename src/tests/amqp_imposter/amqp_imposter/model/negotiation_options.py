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

from dataclasses import dataclass

from ..helpers.nonempty_tuple import NonEmptyTuple
from .constants import (
    DEFAULT_BASIC_NACK,
    DEFAULT_CHANNEL_MAX,
    DEFAULT_CONSUMER_CANCEL_NOTIFY,
    DEFAULT_FRAME_MAX,
    DEFAULT_HEARTBEAT,
    DEFAULT_LOCALES,
    DEFAULT_MECHANISMS,
    DEFAULT_PUBLISHER_CONFIRMS,
)


@dataclass(frozen=True, slots=True)
class NegotiationOptions:
    """
    Configures how the server initiates ConnectionStart and ConnectionTune.
    The server will only accept exact matches of these values in the replies.
    """

    mechanisms: NonEmptyTuple[str] = DEFAULT_MECHANISMS
    locales: NonEmptyTuple[str] = DEFAULT_LOCALES
    publisher_confirms: bool = DEFAULT_PUBLISHER_CONFIRMS
    basic_nack: bool = DEFAULT_BASIC_NACK
    consumer_cancel_notify: bool = DEFAULT_CONSUMER_CANCEL_NOTIFY
    channel_max: int = DEFAULT_CHANNEL_MAX
    frame_max: int = DEFAULT_FRAME_MAX
    heartbeat: int = DEFAULT_HEARTBEAT


DEFAULT_NEGOTIATION_OPTIONS = NegotiationOptions()
