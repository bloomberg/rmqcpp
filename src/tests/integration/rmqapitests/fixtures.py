"""Copyright 2020-2023 Bloomberg Finance L.P.
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
import os
import re
import socket
from collections.abc import Generator
from contextlib import contextmanager, suppress
from dataclasses import dataclass, fields, replace
from typing import TYPE_CHECKING, Any

import pytest
import requests
from requests.auth import HTTPBasicAuth

if TYPE_CHECKING:
    from .platforms import Platform

logger = logging.getLogger(__name__)


class ApiError(Exception):
    """Raised when a management API request returns an error status."""


DEFAULT_HOSTNAME = "rabbit"
DEFAULT_VHOST = "rmq-vhost"


@dataclass
class TestConfig:
    __test__ = False

    exe: str = ""
    hostname: str = ""
    mgmt: str = ""
    user_mgmt: str = ""
    pwd_mgmt: str = ""
    port: str = "30424"
    vhost: str = ""
    user: str = ""
    pwd: str = ""
    proxy_control_port: str = "30454"
    amqpprox_backend_name: str = "rabbit"


def config_from_env() -> TestConfig:
    hostname = os.environ.get("RMQ_HOSTNAME", DEFAULT_HOSTNAME)
    vhost = os.environ.get("RMQ_VHOST", DEFAULT_VHOST)
    derived = TestConfig(
        exe=os.environ["RMQ_EXE"],
        hostname=hostname,
        mgmt=f"http://{hostname}:30439",
        user_mgmt=vhost,
        pwd_mgmt=vhost,
        vhost=vhost,
        user=vhost,
        pwd=vhost,
    )
    overrides = {
        field.name: os.environ[env_name].strip()
        for field in fields(TestConfig)
        if (env_name := f"RMQ_{field.name.upper()}") in os.environ
    }
    return replace(derived, **overrides)


class Api:
    def __init__(self, config):
        self.config = config

    def call_api(self, url: str, method, not_found_allowed, *args, **kwargs) -> Any:
        url = self.config.mgmt + url
        r = requests.request(
            method,
            url,
            *args,
            auth=HTTPBasicAuth(self.config.user_mgmt, self.config.pwd_mgmt),
            **kwargs,
            timeout=10,
        )
        logger.info(f"{method} {url} {r.status_code}")
        logger.info(r.text)
        if not_found_allowed:
            if r.status_code == 404:
                return r
        elif r.status_code >= 400:
            raise ApiError(
                f"http request (code: {r.status_code}) failed with body: {r.text}"
            )
        return r

    def get(self, url: str, not_found_allowed=False, **kwargs) -> Any:
        return self.call_api(
            url, method="GET", not_found_allowed=not_found_allowed, **kwargs
        )

    def post(self, url: str, not_found_allowed=False, **kwargs) -> Any:
        return self.call_api(
            url, method="POST", not_found_allowed=not_found_allowed, **kwargs
        )

    def put(self, url: str, not_found_allowed=False, **kwargs) -> Any:
        return self.call_api(
            url, method="PUT", not_found_allowed=not_found_allowed, **kwargs
        )

    def delete(self, url: str, not_found_allowed=False, **kwargs) -> Any:
        return self.call_api(
            url, method="DELETE", not_found_allowed=not_found_allowed, **kwargs
        )


def send_prox_msg(hostname: str, port: int, msg: str) -> str:
    logger.info(f"sending to proxy: {msg}")
    with socket.create_connection((hostname, port)) as sock:
        sock.sendall(msg.encode("ascii"))
        sock.shutdown(socket.SHUT_WR)
        data = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
        return data.decode("ascii")


@pytest.fixture(scope="session")
def platform(request: pytest.FixtureRequest) -> Platform:
    return request.config.hook.pytest_rmqapitests_platform(config=request.config)


@pytest.fixture(autouse=True)
def _bind_platform(request: pytest.FixtureRequest, platform: Platform) -> None:
    request.cls.platform = platform
    request.cls.config = platform.config
    request.cls.api = Api(platform.config)


@pytest.fixture(autouse=True)
def _adapter_log_level(request: pytest.FixtureRequest) -> None:
    """Put the level where start_perftest_executable can find it.

    Only a level the user asked for arrives as the ``--log-level`` option; the
    INFO default is an ini value. So ``None`` here means "leave
    the adapter on its own default".

    Its own fixture rather than a line in ``vhost``, which test classes
    override.
    """
    request.cls.adapter_log_level = request.config.getoption("log_level")


@pytest.fixture
def prox(request, vhost, platform):
    config = platform.config

    class ProxControl:
        def send_prox_msg(self, msg: str) -> str:
            return send_prox_msg(config.hostname, int(config.proxy_control_port), msg)

        def force_close_connections(self) -> int:
            connections: str = self.send_prox_msg("conn print\n")
            for conn in connections.splitlines():
                match = re.search(r"(?:^\s*|id=)(\d+):? vhost=(\S+)", conn)

                if match:
                    conn_id = match.group(1)
                    connVhost = match.group(2)
                    if connVhost == vhost:
                        logger.info(f"force closing connection: {conn}")
                        self.send_prox_msg(f"session {conn_id} force_disconnect\n")

        def mapVhost(self):
            logger.info(f"Mapping in proxy {vhost}")
            self.send_prox_msg(f"map backend {vhost} {config.amqpprox_backend_name}\n")

        def unmapVhost(self):
            logger.info(f"Removing mapping in proxy {vhost}")
            self.send_prox_msg(f"map unmap {vhost}\n")

        @contextmanager
        def prox_unmapped(self):
            try:
                self.unmapVhost()
                yield None
            finally:
                self.mapVhost()

    prox = ProxControl()
    request.cls.prox = prox
    return prox


def short_test_id(nodeid: str) -> str:
    """A pytest node id without its file path: ``Class::test[params]``.

    What both the framework's own log lines and the adapter output it forwards
    are tagged with, so one grep matches both.
    """
    return nodeid.split("::", 1)[-1]


def capability_of(item: pytest.Item) -> str | None:
    marker = item.get_closest_marker("capability")
    return marker.args[0] if marker else None


_vhost_counters: dict[str, int] = {}


@pytest.fixture(autouse=True)
def vhost(request):
    capability = capability_of(request.node)
    _vhost_counters[capability] = _vhost_counters.get(capability, 0) + 1
    vhost = f"rmq-conformance-{capability}-{_vhost_counters[capability]}"
    logger.info(f"vhost mapping: {vhost} -> {request.node.nodeid}")
    request.cls.vhost = vhost
    return vhost


@pytest.fixture(autouse=True)
def vhost_on_broker(prox: Any, vhost: str, platform: Platform) -> Generator[str]:
    api = Api(platform.config)
    rcode = api.put(f"/api/vhosts/{vhost}").status_code
    assert rcode == 201 or rcode == 204

    platform.grant_access(api, vhost)

    prox.mapVhost()
    yield vhost
    prox.unmapVhost()
    with suppress(Exception):
        platform.revoke_access(api, vhost)
        api.delete(f"/api/vhosts/{vhost}")
