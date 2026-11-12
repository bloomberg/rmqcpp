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

from amqp_imposter.amqp import transcript as tr


def make_transcript(max_lines: int, count: int) -> tr.Transcript:
    transcript = tr.Transcript(max_lines=max_lines)

    for _ in range(count):
        transcript.add(
            micros=0,
            epoch_micros=0,
            event=tr.TranscriptEvent.TCP_CONNECTED,
        )
    return transcript


def test_indices() -> None:
    transcript = make_transcript(max_lines=tr.DEFAULT_MAX_TRANSCRIPT_LINES, count=3)

    assert [line.index for line in transcript.lines] == [0, 1, 2]


def test_recorded() -> None:
    transcript = make_transcript(max_lines=2, count=5)

    assert transcript.recorded == 5


def test_drop_oldest() -> None:
    transcript = make_transcript(max_lines=2, count=5)

    assert [line.index for line in transcript.lines] == [3, 4]


def test_under_cap() -> None:
    transcript = make_transcript(max_lines=tr.DEFAULT_MAX_TRANSCRIPT_LINES, count=50)

    assert len(transcript.lines) == 50


@pytest.mark.parametrize("max_lines", (1, 3, 10, tr.DEFAULT_MAX_TRANSCRIPT_LINES))
def test_first_index(max_lines: int) -> None:
    transcript = make_transcript(max_lines=max_lines, count=10)

    first = transcript.lines[0].index

    assert first == transcript.recorded - len(transcript.lines)
