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

from collections import defaultdict
from collections.abc import Callable, Iterable
from dataclasses import dataclass

import pytest
from google.protobuf.message import Message

from amqp_imposter.control.client import (
    Transcript,
    format_transcript_around,
    pb,
    transcript_line_method,
)


@dataclass(frozen=True, slots=True)
class Failure:
    reason: str
    line_index: int | None


type Property = Callable[[Transcript], Failure | None]


def event_and_method(
    line: pb.TranscriptLine,
) -> tuple[pb.TranscriptEvent.ValueType, Message] | None:
    if (method := transcript_line_method(line)) is not None:
        return line.event, method
    return None


def assert_properties(transcript: Transcript, properties: Iterable[Property]) -> None:
    reasons_by_line_index: defaultdict[int | None, list[str]] = defaultdict(list)
    for failure in filter(None, (prop(transcript) for prop in properties)):
        reasons_by_line_index[failure.line_index].append(failure.reason)

    if reasons_by_line_index:
        pytest.fail(
            "\n\n".join(
                "\n".join((*reasons, format_transcript_around(transcript, line_index)))
                for line_index, reasons in reasons_by_line_index.items()
            ),
            pytrace=False,
        )
