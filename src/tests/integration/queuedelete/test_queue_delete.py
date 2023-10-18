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
from time import sleep


# TODO: This test cannot be supported by rmqapitests
# perfTest can tare down queues when the tests are over by declaring them as auto-delete
# but cannot test deleting a specific queue (because its a performance tester, not a functionality tester)
class TestQueueDelete(RmqTest):
    def test_delete_queue(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        queue = "test_delete_queue"
        self.create_queue(queue)
        message = {
            "properties": {},
            "routing_key": queue,
            "payload": "hello world",
            "payload_encoding": "string",
        }
        self.publish_message(message)

        assert self.timed_expect_true(lambda: self.get_queue_info(queue) is not None)
        assert self.timed_expect_true(
            lambda: self.get_queue_info(queue).get("messages") == 1
        )

        p = self.start_perftest_executable(
            [
                "--queue",
                queue,
            ]
        )
        assert self.timed_expect_true(lambda: self.get_queue_info(queue) is None)

        assert p.poll() != None  # process terminated

    def test_delete_queue_does_not_exist(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)

        p = self.start_perftest_executable(["--queue", "this-queue-does-not-exist"])

        assert self.timed_expect_true(lambda: p.poll() != None)  # process terminated

    def test_delete_queue_fails_if_used(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        queue = "test_delete_queue_fails_if_used"
        self.create_queue(queue)

        assert self.timed_expect_true(lambda: self.get_queue_info(queue) is not None)

        # attach a consumer
        consumer = self.start_perftest_executable(
            [
                "--auto-delete",
                "false",
                "--queue",
                queue,
                "--consumers",
                "1",
                "--producers",
                "0",
                "--flag",
                "persistent",
            ],
            alt_exe=True
        )

        assert self.timed_expect_true(
            lambda: self.get_queue_info(queue).get("consumers") == 1
        )

        # try to delete the queue with a consumer attached
        p = self.start_perftest_executable(["--ifUnused", "--queue", queue])
        assert not self.timed_expect_true(lambda: self.get_queue_info(queue) is None)

        assert p.poll() == None  # process not terminated
        p.kill()
        consumer.kill()
        # cleanup test queue
        self.delete_queue_if_exists(queue)

    def test_delete_queue_if_unused(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        queue = "test_delete_queue_if_unused"
        self.create_queue(queue)

        assert self.timed_expect_true(
            lambda: self.get_queue_info(queue).get("consumers") == 0
        )

        p = self.start_perftest_executable(["--ifUnused", "--queue", queue])
        assert self.timed_expect_true(lambda: self.get_queue_info(queue) is None)

        assert p.poll() != None  # process terminated
        # cleanup test queue
        self.delete_queue_if_exists(queue)

    def test_delete_queue_if_empty(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        queue = "test_delete_queue_if_empty"
        self.create_queue(queue)
        message = {
            "properties": {},
            "routing_key": queue,
            "payload": "hello world",
            "payload_encoding": "string",
        }
        self.publish_message(message)

        assert self.timed_expect_true(lambda: self.get_queue_info(queue) is not None)
        assert self.timed_expect_true(
            lambda: self.get_queue_info(queue).get("messages") == 1
        )

        p = self.start_perftest_executable(["--ifEmpty", "--queue", queue])
        assert not self.timed_expect_true(lambda: self.get_queue_info(queue) is None)

        assert p.poll() == None  # process not terminated
        p.kill()

        self.purge_queue(queue)
        assert self.timed_expect_true(
            lambda: self.get_queue_info(queue).get("messages") == 0
        )
        p = self.start_perftest_executable(["--ifEmpty", "--queue", queue])
        assert self.timed_expect_true(lambda: self.get_queue_info(queue) is None)

        assert p.poll() != None  # process terminated
