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
from ..rmqapitests.fixtures import *
from ..rmqapitests.tests_runner import RmqTest


# TODO: This test cannot be supported by rmqapitests
# perfTest has no ability to create a specified number of connections in a single instance
class TestProduceMessage(RmqTest):
    def test_multithread_produce(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "create-multithread-producer-queue"

        # start executable, publish N messages
        N = 10
        MAX_OUTSTANDING = 5
        PRODUCERS_PER_CONNECTION = 5
        NUMBER_OF_CONNECTIONS = 5
        self.start_perftest_executable(
            [
                "--queue",
                qname,
                "--send",
                str(N),
                "--sleepPeriod",
                "0",
                "--maxOutstanding",
                str(MAX_OUTSTANDING),
                "--numberOfProducers",
                str(PRODUCERS_PER_CONNECTION),
                "--numberOfConnections",
                str(NUMBER_OF_CONNECTIONS),
            ]
        )

        assert self.timed_expect_true(
            lambda: self.get_message_count(qname)
            == NUMBER_OF_CONNECTIONS * PRODUCERS_PER_CONNECTION * N,
            timeout=60,
        )
