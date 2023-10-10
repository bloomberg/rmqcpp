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
from .tests_runner import RmqTest
from .fixtures import *

MESSAGE_COUNT = 100


class TestConsumeNMessage(RmqTest):
    def test_consume_n_messages(self):
        """
        Confirm that a single consumer can process exactly 50 messages and then stop.

        Confirm 10 messages are left over.

        Passes:
        --auto-delete false
        --queue test-cancel-consumer-queue
        --consumers 1
        --cmessages 50
        --producers 0
        --uri <amqp endpoint>
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "test-consume-n-messages"
        self.start_perftest_executable(
            [
                "--auto-delete",
                "false",
                "--queue",
                qname,
                "--consumers",
                "1",
                "--cmessages",
                "50",
                "--producers",
                "0",
            ]
        )
        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")

        assert self.wait_for_n_consumers(qname, 1)
        assert self.get_message_count(qname) == 0

        message = {
            "properties": {},
            "routing_key": qname,
            "payload": "good",
            "payload_encoding": "string",
        }
        print("Starting producer.")
        for _ in range(60):
            response = self.publish_message(message)
            assert response["routed"] is True

        if not self.wait_for_message_consumption(
            qname, 10, success_on_queue_delete=True
        ):
            pytest.fail("Consumer is not able to consume the message.")


class TestConsumeParameters(RmqTest):
    """
    Test that five (5) separate consumers are started on five (5) separate queues and each consume from each.
    Each queue created should have one (1) consumer.

    Passes:
    --qos 10
    --consumers 5
    --queue-pattern cpq
    --queue-pattern-from 1
    --queue-pattern-to 5
    --uri <amqp endpoint>
    """

    def test_consume(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "cpq"
        consumers = [1, 2, 3, 4, 5]
        consumer_queue_names = []
        for i in consumers:
            consumer_queue_names.append(f"{qname}-{i}")
        print(f"{consumer_queue_names}")
        self.start_perftest_executable(
            [
                "--auto-delete",
                "false",
                "--queue-pattern",
                qname,
                "--qos",
                "10",
                "--consumers",
                "5",
                "--queue-pattern-from",
                "1",
                "--queue-pattern-to",
                "5",
                "--producers",
                "0",
            ]
        )

        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")

        for queue in consumer_queue_names:
            print(f"waiting for a consumer on {queue}")
            assert self.wait_for_n_consumers(queue, 1)

        for _ in range(MESSAGE_COUNT):
            for queue in consumer_queue_names:
                message = {
                    "properties": {},
                    "routing_key": queue,
                    "payload": "hello world",
                    "payload_encoding": "string",
                }
                response = self.publish_message(message)

        for queue in consumer_queue_names:
            message = {
                "properties": {},
                "routing_key": queue,
                "payload": "exit",
                "payload_encoding": "string",
            }
            response = self.publish_message(message)
            assert response["routed"] is True

        for queue in consumer_queue_names:
            if not self.wait_for_message_consumption(
                queue, success_on_queue_delete=True
            ):
                pytest.fail(
                    f"Consumer is not able to consume the messages from {queue}"
                )


class TestConsumeMessage(RmqTest):
    """
    Test consuming a message from a queue

    Passes:
    --queue create-consumer-queue
    --consumers 1
    --producers 0
    --uri <amqp endpoint>

    """

    def test_consume(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "test-consume-message"
        self.start_perftest_executable(
            ["--queue", qname, "--consumers", "1", "--producers", "0"]
        )

        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")

        assert self.wait_for_n_consumers(qname, 1)
        assert self.get_message_count(qname) == 0

        message = {
            "properties": {},
            "routing_key": qname,
            "payload": "hello world",
            "payload_encoding": "string",
        }
        response = self.publish_message(message)
        assert response["routed"] is True

        if not self.wait_for_message_consumption(qname, success_on_queue_delete=True):
            pytest.fail("Consumer is not able to consume the message.")


class TestConsumeMessageWithPriority(RmqTest):
    """
    Starts two instances of the executable in five (5) scenarios.

    It is expected that the high-priority-consumer fully disconnects once it has finished processing all messages as
    indicated by the `--cmessages` argument

    Passes:
    --auto-delete false
    --queue create-consumer-queue
    --consumers 1
    --consumer-args  "x-priority=0"
    --cmessages 1
    --producers 0
    --flag persistent
    --uri <amqp endpoint>

    And:
    --auto-delete false
    --queue create-consumer-queue
    --consumers 1
    --consumer-args  "x-priority=1"
    --cmessages 5
    --producers 0
    --flag persistent
    --uri <amqp endpoint>
    """

    @pytest.mark.parametrize(
        "low_priority, high_priority",
        [
            (0, 1),
            (-2, 1),
            (-255, 255),
            (2**63 - 2, 2**63 - 1),
            (-(2**63) + 1, -(2**63) + 2),
        ],
    )
    def test_consume(self, low_priority, high_priority):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "test-consumer-priority"
        num_messages = 5

        # TODO: why does this break when 'durable' queues are not used?
        self.start_perftest_executable(
            [
                "--auto-delete",
                "false",
                "--queue",
                qname,
                "--consumers",
                "1",
                "--consumer-args",
                f"x-priority={low_priority}",
                "--cmessages",
                "1",
                "--producers",
                "0",
                "--flag",
                "persistent",
            ]
        )
        self.start_perftest_executable(
            [
                "--auto-delete",
                "false",
                "--queue",
                qname,
                "--consumers",
                "1",
                "--consumer-args",
                f"x-priority={high_priority}",
                "--cmessages",
                f"{num_messages}",
                "--producers",
                "0",
                "--flag",
                "persistent",
            ]
        )
        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")

        assert self.wait_for_n_consumers(qname, 2)

        for _ in range(num_messages + 1):
            assert self.get_message_count(qname) == 0

            message = {
                "properties": {},
                "routing_key": qname,
                "payload": "hello world",
                "payload_encoding": "string",
            }
            response = self.publish_message(message)
            assert response["routed"] is True

            if not self.wait_for_message_consumption(
                qname, success_on_queue_delete=True
            ):
                pytest.fail("Consumer is not able to consume the message.")

        # use timed_expect_true() in case there is a delay between the message ack and consumer destruction
        print("testing high priority consumer is gone")
        assert self.timed_expect_true(
            lambda: not self.exist_priority_consumer(qname, high_priority), timeout=20
        )
        print("testing low priority consumer still exists")
        # low priority consumer should not be destructed because it should not have consumed a message
        assert self.exist_priority_consumer(qname, low_priority)
