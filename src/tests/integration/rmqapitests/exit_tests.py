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
from time import sleep
from .tests_runner import RmqTest


class TestExits(RmqTest):
    def test_both_exit_with_available_vhost(self):
        """
        Test both the consumer and producer exit after the expected
        number of messages has been produced and consumed.
        """

        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "test-both-exit-queue"
        exch = "my-exchange"

        N = 1

        self.start_perftest_executable(
            [
                "--auto-delete",
                "false",
                "--exchange",
                exch,
                "--queue",
                qname,
                "--pmessages",
                str(N),
                "--producers",
                "1",
                "--consumers",
                "0",
            ]
        )

        # wait for the producer to exit
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)

        # confirm the message is there.
        assert self.wait_for_n_published(qname, N)

        # consume the messages
        self.start_perftest_executable(
            [
                "--auto-delete",
                "false",
                "--exchange",
                exch,
                "--queue",
                qname,
                "--cmessages",
                str(N),
                "--consumers",
                "1",
                "--producers",
                "0",
            ]
        )

        # wait for the consumer to exit
        assert self.timed_expect_true(
            lambda: self.get_connection_count() == 0, timeout=30
        )

        # assert the queue is now empty
        if not self.wait_for_message_consumption(qname, success_on_queue_delete=True):
            pytest.fail(f"Consumer is not able to consume the messages from {qname}")
        # cleanup
        self.delete_queue_if_exists(qname)
