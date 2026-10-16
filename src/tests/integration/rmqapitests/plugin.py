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

import logging
from typing import Any

import pytest

from .fixtures import (  # noqa: F401 — re-export fixtures for pytest discovery
    _adapter_log_level,
    _bind_platform,
    capability_of,
    config_from_env,
    platform,
    prox,
    short_test_id,
    vhost,
    vhost_on_broker,
)
from .platforms import Platform


class PlatformHookSpec:
    @pytest.hookspec(firstresult=True)
    def pytest_rmqapitests_platform(self, config: pytest.Config) -> Platform:
        raise NotImplementedError


def pytest_addhooks(pluginmanager: pytest.PytestPluginManager) -> None:
    pluginmanager.add_hookspecs(PlatformHookSpec)


@pytest.hookimpl(trylast=True)
def pytest_rmqapitests_platform(config: pytest.Config) -> Platform:
    return Platform(config_from_env())


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--capability",
        action="extend",
        nargs="+",
        default=[],
        metavar="NAME",
        help="Run only tests with these capabilities (default: all)",
    )


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line(
        "markers", "capability(name): the conformance capability a test belongs to"
    )


@pytest.hookimpl(tryfirst=True)
def pytest_collection_modifyitems(
    config: pytest.Config, items: list[pytest.Item]
) -> None:
    untagged = [item.nodeid for item in items if capability_of(item) is None]
    if untagged:
        raise pytest.UsageError(
            "Tests without a capability marker: " + ", ".join(untagged)
        )

    requested = set(config.getoption("capability"))
    if not requested:
        return

    known = {capability_of(item) for item in items}
    unknown = requested - known
    if unknown:
        raise pytest.UsageError(
            f"Unknown capability: {', '.join(sorted(unknown))}. "
            f"Known: {', '.join(sorted(c for c in known if c))}"
        )

    selected = [item for item in items if capability_of(item) in requested]
    deselected = [item for item in items if capability_of(item) not in requested]
    config.hook.pytest_deselected(items=deselected)
    items[:] = selected


class _CurrentTestId:
    """The test currently running, stamped onto every log record."""

    current = "-"


_base_record_factory = logging.getLogRecordFactory()


def _test_id_record_factory(*args: Any, **kwargs: Any) -> logging.LogRecord:
    record = _base_record_factory(*args, **kwargs)
    record.test_id = _CurrentTestId.current
    return record


logging.setLogRecordFactory(_test_id_record_factory)


def pytest_runtest_logstart(nodeid: str, location: Any) -> None:
    _CurrentTestId.current = short_test_id(nodeid)


def pytest_runtest_logfinish(nodeid: str, location: Any) -> None:
    _CurrentTestId.current = "-"
