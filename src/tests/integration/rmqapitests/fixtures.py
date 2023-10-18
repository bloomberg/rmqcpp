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
import pytest
import os
from enum import Enum
import requests
from requests.auth import HTTPBasicAuth
from typing import Any
from telnetlib import Telnet
from contextlib import contextmanager
import logging
import re
from pathlib import Path


CERT_PATH = Path(__file__).parent / "certs"

DEFAULT_VHOST = "rmqcpp"


class TlsConfig(Enum):
    NONE = 0
    VERIFY_SERVER = 1
    VERIFY_MUTUAL = 2


class TestConfig:
    exe: str = ""
    management: str = ""
    user_mgmt: str = ""
    pwd_mgmt: str = ""
    hostname: str = ""
    port: str = ""
    vhost: str = ""
    user: str = ""
    pwd: str = ""
    proxy_control_port: str = ""
    ca_cert: str = ""
    client_cert: str = ""
    client_key: str = ""
    tls_port: str = ""
    tls: TlsConfig = TlsConfig.NONE


@pytest.fixture(scope="class")
def config(request) -> TestConfig:
    args = {
        "exe": None,  # Required
        "alt_exe": "",
        "mgmt": "http://localhost:15672",
        "user_mgmt": os.environ.get("RMQ_VHOST", DEFAULT_VHOST),
        "pwd_mgmt": os.environ.get("RMQ_VHOST", DEFAULT_VHOST),
        "hostname": os.environ.get("RMQ_HOSTNAME","localhost"),
        "port": os.environ.get("RMQ_PORT","5672"),
        "tls_port": os.environ.get("RMQ_TLS_PORT","5671"),
        "user": os.environ.get("RMQ_VHOST", DEFAULT_VHOST),
        "pwd": os.environ.get("RMQ_VHOST", DEFAULT_VHOST),
        "proxy_control_port": "5700",
        "ca_cert": str(CERT_PATH / "ca.crt"),
        "client_cert": str(CERT_PATH / "client.crt"),
        "client_key": str(CERT_PATH / "client.key"),
    }

    print(os.getcwd())

    config = TestConfig()
    for arg, default in args.items():
        setattr(config, arg, os.environ.get(f"RMQ_{arg.upper()}", default).strip())
    request.cls.config = config
    return config


@pytest.fixture(scope="class")
def api(request, config):
    class Api:
        def call_api(self, url: str, method, not_found_allowed, *args, **kwargs) -> Any:
            r = requests.request(
                method,
                config.mgmt + url,
                *args,
                auth=HTTPBasicAuth(config.user_mgmt, config.pwd_mgmt),
                **kwargs,
                timeout=10,
            )
            print(f"{method} {config.mgmt}{url} {r.status_code} {r.text}")
            if not_found_allowed:
                if r.status_code == 404:
                    return r
            elif r.status_code >= 400:
                raise Exception(
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

    api = Api()
    request.cls.api = api
    return api


@pytest.fixture
def prox(config, request, vhost):
    class ProxControl:
        def send_prox_msg(self, msg: bytes) -> bytes:
            logging.info(f"sending to proxy: {msg}")
            with Telnet(config.hostname, config.proxy_control_port) as tn:
                tn.write(msg.encode("ascii"))
                return tn.read_all().decode("ascii")

        def force_close_connections(self) -> int:
            connections: str = self.send_prox_msg("conn print\n")
            for conn in connections.splitlines():
                match = re.search(r"(\d+): vhost=(\S+)", conn)

                if match:
                    id = match.group(1)
                    connVhost = match.group(2)
                    if connVhost == vhost:
                        logging.info(f"force closing connection: {conn}")
                        self.send_prox_msg(f"session {id} force_disconnect\n")

        def mapVhost(self):
            logging.info(f"Mapping in proxy {vhost}")
            self.send_prox_msg(f"map backend {vhost} rabbit\n")

        def unmapVhost(self):
            logging.info(f"Removing mapping in proxy {vhost}")
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


@pytest.fixture(autouse=True)
def vhost(request):
    vhost = request.module.__name__ + "::" + request.function.__name__
    request.cls.vhost = vhost
    return vhost


@pytest.fixture(autouse=True)
def vhost_on_broker(api, config, prox, vhost):
    rcode = api.put(f"/api/vhosts/{vhost}").status_code
    assert rcode == 201 or rcode == 204

    rcode = api.put(
        f"/api/permissions/{vhost}/{config.user}",
        json={"configure": ".*", "write": ".*", "read": ".*"},
    ).status_code
    assert rcode == 201 or rcode == 204

    prox.mapVhost()
    yield vhost
    prox.unmapVhost()
    try:
        api.delete(f"/api/vhosts/{vhost}")
    except:
        pass
