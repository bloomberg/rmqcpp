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
import os
from time import sleep

import pytest

from .tests_runner import (
    RmqTest,
    SpoofHungServer,
)

DEFAULT_CONNECTION_TEST_ARGS = [
    "--publishing-interval",
    "1",
    "--pmessages",
    "1",
    "--cmessages",
    "2",
]


class TestRmqConnects(RmqTest):
    def test_connects(self):
        """Ensure that starting our executable causes a connection to appear on the broker"""
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        self.start_perftest_executable(DEFAULT_CONNECTION_TEST_ARGS)
        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")


class TestRmqDisconnects(RmqTest):
    def test_connect_disconnect(self):
        """Ensure that starting our executable with a short runtime causes it to disconnect"""
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        self.start_perftest_executable(["--time", "1"])

        # Assert we disconnect after a certain period:
        timeout = 60  # seconds
        step = 0.5  # seconds
        assert self.timed_expect_true(
            lambda: self.get_connection_count() == 0, timeout, step
        )


class TestRmqConnectAfterStartingUnavailable(RmqTest):
    def test_unavailable_connect(self):
        """Ensure the application does eventually connect even if the first connection should fail"""
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)

        with self.prox.prox_unmapped():
            self.start_perftest_executable(DEFAULT_CONNECTION_TEST_ARGS)
        if not self.wait_for_connection():
            pytest.fail("Failed due to client not reconnecting")


class TestRmqForcefulReconnect(RmqTest):
    def test_forceful_disconnect(self):
        """Ensure that a force disconnect triggers the client to reconnect"""
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        self.start_perftest_executable(DEFAULT_CONNECTION_TEST_ARGS)
        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")
        self.prox.force_close_connections()
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        if not self.wait_for_connection():
            pytest.fail("Failed due to client not reconnecting")


class TestRmqGracefulReconnect(RmqTest):
    def test_graceful_disconnect(self):
        """Ensure that a graceful disconnect triggers the client to reconnect"""
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        self.start_perftest_executable(DEFAULT_CONNECTION_TEST_ARGS)

        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")

        self.close_all_connections()
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)

        if not self.wait_for_connection():
            pytest.fail("Failed due to client not reconnecting")


class TestRmqRetryUponTcpTimeout(RmqTest):
    def test_retry_at_tcp_timeout(self):
        """
        Ensure that the client times out and eventually retries connecting even when
        the connection target does not accept()/syn-ack the connection.
        """
        if not self.linux():
            print("Non-linux os detected, test only runs on linux, exit ok.")
            return

        self.config.hostname = "localhost"
        spoofer = SpoofHungServer()
        rmq_port = os.getenv("RMQ_PORT") or 5672
        rmq_host = os.getenv("RMQ_HOSTNAME") or "amqpprox"
        try:
            print("start spoof")
            spoofer.start_hung_server()
            self.config.port = spoofer.spoof_port
            self.start_perftest_executable(DEFAULT_CONNECTION_TEST_ARGS)
            sleep(15)  # give the spoof thread a second to accept a connection
            spoofer.assert_spoofed()
            # after asserting the spoof has connected confirm the rabbit broker has none
            assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
            # Now forward to a good endpoint
            spoofer.start_forwarding(rmq_host, int(rmq_port))
            if not self.wait_for_connection():
                pytest.fail("Failed due to client not reconnecting")
        finally:
            spoofer.stop()
            self.config.hostname = (
                rmq_host  # set hostname back to default for cleanup steps
            )
            self.config.port = rmq_port


class TestConsumeAndProduceOnSeparateConnections(RmqTest):
    def test_separate_connections(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)

        self.start_perftest_executable(
            [
                "--queue",
                "multi-connect",
                "--producers",
                "1",
                "--consumers",
                "1",
                "--publishing-interval",
                "1",
            ]
        )

        assert self.timed_expect_true(
            lambda: self.get_connection_count() == 2, timeout=60
        )
