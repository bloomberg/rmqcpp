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
class TestConsumeMessage(RmqTest):
    def test_consume(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "create-multithread-consumer-queue"
        consumer_tag = "my-consumer-tag"
        CONSUMERS_PER_CONNECTION = 5
        NUMBER_OF_CONNECTIONS = 2
        self.delete_queue_if_exists(qname)
        self.start_perftest_executable(
            [
                "--queue",
                qname,
                "--tag",
                consumer_tag,
                "--numberOfConsumers",
                str(CONSUMERS_PER_CONNECTION),
                "--numberOfConnections",
                str(NUMBER_OF_CONNECTIONS),
            ]
        )

        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")

        assert self.timed_expect_true(
            lambda: self.get_channel_count()
            == NUMBER_OF_CONNECTIONS * CONSUMERS_PER_CONNECTION,
            timeout=60,
        )

        for i in range(CONSUMERS_PER_CONNECTION):
            for j in range(NUMBER_OF_CONNECTIONS):
                assert self.wait_for_consumer(
                    consumer_tag + str(i) + "_" + str(j), qname
                )
        assert self.get_connection_count() == NUMBER_OF_CONNECTIONS
        assert self.get_message_count(qname) == 0

        for i in range(CONSUMERS_PER_CONNECTION):
            for j in range(NUMBER_OF_CONNECTIONS):
                message = {
                    "properties": {},
                    "routing_key": qname,
                    "payload": "hello world",
                    "payload_encoding": "string",
                }
                response = self.publish_message(message)
                assert response["routed"] == True

        if not self.wait_for_message_consumption(qname, success_on_queue_delete=True):
            pytest.fail("Consumer is not able to consume the message.")
