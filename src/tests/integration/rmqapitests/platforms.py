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

from collections.abc import Callable, Generator
from contextlib import contextmanager
from dataclasses import dataclass

from .fixtures import Api, TestConfig
from .integration_test import SpoofHungServer


class HostHealthDriver:
    """Makes the host look healthy or unhealthy to the adapter under test.

    How a host reports its health belongs to where it runs, so this is supplied
    rather than defaulted.
    """

    def setup(self, vhost: str) -> None:
        """Prepare the signal, before the adapter under test starts."""

    def teardown(self) -> None:
        """Release whatever ``setup`` acquired."""

    def set_health(self, healthy: bool) -> None:
        """Put the host into ``healthy`` and confirm it got there.

        An unreadable health source conventionally reports healthy, so a driver
        that fails quietly turns every test here into a vacuous pass.
        """
        raise NotImplementedError


@dataclass
class HungServerDiversion:
    spoofer: SpoofHungServer
    release: Callable[[], object]


class Platform:
    host_health_driver: type[HostHealthDriver] = HostHealthDriver

    def __init__(self, config: TestConfig) -> None:
        self.config = config
        self._hung_server_ports: dict[str, int] = {}

    def connection_args(self, vhost: str) -> list[str]:
        hostname, port = self.config.hostname, self.config.port
        if vhost in self._hung_server_ports:
            hostname, port = "localhost", str(self._hung_server_ports[vhost])
        user, pwd = self.config.user, self.config.pwd
        return ["--uri", f"amqp://{user}:{pwd}@{hostname}:{port}/{vhost}"]

    def adapter_env(self, env: dict[str, str]) -> dict[str, str]:
        return env

    def grant_access(self, api: Api, vhost: str) -> None:
        rcode = api.put(
            f"/api/permissions/{vhost}/{self.config.user}",
            json={"configure": ".*", "write": ".*", "read": ".*"},
        ).status_code
        assert rcode in (201, 204)

    def revoke_access(self, api: Api, vhost: str) -> None:
        pass

    @contextmanager
    def divert_to_hung_server(self, vhost: str) -> Generator[HungServerDiversion]:
        spoofer = SpoofHungServer()
        self._hung_server_ports[vhost] = spoofer.spoof_port
        try:
            spoofer.start_hung_server()
            yield HungServerDiversion(
                spoofer,
                release=lambda: spoofer.start_forwarding(
                    self.config.hostname, int(self.config.port)
                ),
            )
        finally:
            del self._hung_server_ports[vhost]
            spoofer.stop()
