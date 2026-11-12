"""Copyright 2026 Bloomberg Finance L.P.
SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from __future__ import annotations

import os
from collections.abc import Generator
from pathlib import Path

import pytest
from amqp_imposter.control.client import (
    AmqpImposter,
    Transcript,
    amqp_imposter,
)

__all__ = ["imposter", "pytest_runtest_makereport"]

pytest_plugins = ["integration.rmqapitests.xfail"]

IMPOSTER_STASH_KEY = pytest.StashKey[AmqpImposter]()


@pytest.fixture
def imposter(request: pytest.FixtureRequest) -> Generator[AmqpImposter]:
    with amqp_imposter() as ai:
        request.node.stash[IMPOSTER_STASH_KEY] = ai
        yield ai
        del request.node.stash[IMPOSTER_STASH_KEY]


def write_transcript(directory: Path, test_name: str, transcript: Transcript) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    path = directory / f"{test_name}.txt"
    path.write_text(f"{transcript}\n")
    return path


@pytest.hookimpl(wrapper=True)
def pytest_runtest_makereport(
    item: pytest.Item,
) -> Generator[None, pytest.TestReport, pytest.TestReport]:
    report = yield
    transcript_dir = os.environ.get("FUZZ_TRANSCRIPT_DIR")
    imposter = item.stash.get(IMPOSTER_STASH_KEY, None)

    if (
        report.when == "call"
        and report.failed
        and transcript_dir
        and imposter is not None
    ):
        path = write_transcript(Path(transcript_dir), item.name, imposter.transcript())
        report.sections.append(("imposter transcript", str(path)))

    return report
