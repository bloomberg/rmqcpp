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
from subprocess import TimeoutExpired
from .tests_runner import RmqTest
from .fixtures import *


class TestProduceMessage(RmqTest):
    def test_produce(self):
        """
        Test the basic producer connects and publishes N message.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "create-producer-queue"
        exch = "my-exchange"

        # start executable, publish N messages
        N = 10
        MAX_OUTSTANDING = 5

        self.start_perftest_executable(
            [
                "--exchange",
                exch,
                "--queue",
                qname,
                "--pmessages",
                str(N),
                "--consumers",
                str(0),
                "--producers",
                "1",
                "--confirm",
                str(MAX_OUTSTANDING),
                "--routing-key",
                "routing-key",
            ]
        )

        assert self.wait_for_n_published(qname, N)

    def test_multiple_producer_exit_cleanly(self):
        """Ensure we can run the producer sequentially multiple times
        with a clean exit code.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "multi-run-producer"
        exch = "my-exchange"

        NumProducers = 5
        NumMsgPerProducer = 5

        p = self.start_perftest_executable(
            [
                "--queue",
                qname,
                "--exchange",
                exch,
                "--pmessages",
                str(NumMsgPerProducer),
                "--consumers",
                str(0),
                "--producers",
                str(NumProducers),
                "--routing-key",
                "routing-key",
            ]
        )
        try:
            out = p.wait(timeout=120)
            print(f"test_multiple_producer_exit_cleanly rcode: {p.returncode}")
            assert p.returncode == 0
            assert self.wait_for_n_published(qname, NumProducers * NumMsgPerProducer)
        except TimeoutExpired:
            p.kill()
            out = p.communicate()
            print(
                f"Timeout reached waiting for test_multiple_producer_exit_cleanly. Details: {out}"
            )
            assert False


class TestPersistence(RmqTest):
    def test_persistent_message(self):
        """
        This test will soon be deprecated as the persistent flag becomes irrelevant.
        For now it's read by classic queues.
        Uses default exchange, with default routing key.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "publish-persistent-messages"

        N = 5

        self.start_perftest_executable(
            [
                "--queue",
                qname,
                "--pmessages",
                str(N),
                "--consumers",
                str(0),
                "--producers",
                "1",
                "--flag",
                "persistent",
            ]
        )

        assert self.timed_expect_true(
            lambda: self.get_persistent_message_count(qname) == N, 600
        )

    def test_default_non_persistent_message(self):
        """
        This test will soon be deprecated as the persistent flag becomes irrelevant.
        For now it's read by classic queues
        """

        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "publish-non-persistent-messages"

        N = 5

        self.start_perftest_executable(
            [
                "--queue",
                qname,
                "--pmessages",
                str(N),
                "--consumers",
                str(0),
                "--producers",
                "1",
                "--routing-key",
                qname,
            ]
        )
        assert self.timed_expect_true(
            lambda: self.get_persistent_message_count(qname) == 0
        )
        assert self.timed_expect_true(lambda: self.get_message_count(qname) == N)


class TestProducerWithBrokerUnavailable(RmqTest):
    def test_unavailable_producer(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)

        with self.prox.prox_unmapped():
            qname = "create-producer-queue"
            N = 5
            self.start_perftest_executable(
                [
                    "--queue",
                    qname,
                    "--producers",
                    "1",
                    "--consumers",
                    "0",
                    "--pmessages",
                    str(N),
                ]
            )

        assert self.wait_for_n_published(qname, N)
