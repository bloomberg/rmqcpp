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

from fnmatch import fnmatchcase
from pathlib import Path

import pytest

from .fixtures import short_test_id

_PACKAGE_ROOT = Path(__file__).parent


def package_relative_test_id(item: pytest.Item) -> str:
    try:
        path = item.path.relative_to(_PACKAGE_ROOT).as_posix()
    except ValueError:
        return item.nodeid
    return f"{path}::{short_test_id(item.nodeid)}"


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--xfail",
        action="append",
        default=[],
        metavar="GLOB",
        dest="xfail_globs",
        help="Expect tests whose package-relative id matches GLOB to fail. May be repeated.",
    )
    parser.addoption(
        "--xfail-reason",
        default="Not yet supported by this client library",
        metavar="REASON",
        help="Reason attached to tests selected by --xfail",
    )


def pytest_collection_modifyitems(
    config: pytest.Config, items: list[pytest.Item]
) -> None:
    globs: list[str] = config.getoption("xfail_globs")
    if not globs:
        return
    marker = pytest.mark.xfail(reason=config.getoption("xfail_reason"))
    for item in items:
        test_id = package_relative_test_id(item)
        if any(test_id == glob or fnmatchcase(test_id, glob) for glob in globs):
            item.add_marker(marker)
