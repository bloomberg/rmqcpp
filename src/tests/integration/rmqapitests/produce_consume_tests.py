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

class TestProduceConsumeMessages(RmqTest):
    def test_cancel_consume_after_n_messages(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "cancel-consumer-queue"
        self.start_perftest_executable(
            [
                "--auto-delete",
                "false",
                "--queue",
                qname,
                "--cmessages",
                "50",
                "--pmessages",
                "100",
            ]
        )

        if not self.wait_for_message_consumption(
            qname, 50, success_on_queue_delete=True
        ):
            pytest.fail("Consumer is not able to consume the message.")
