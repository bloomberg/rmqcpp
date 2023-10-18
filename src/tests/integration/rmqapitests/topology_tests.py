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
from .tests_runner import RmqTest


class TestTopology(RmqTest):
    def test_topology(self):
        """
        Tests each standard exchange type can be created.
        Tests that queues can be created and bound to each standard exchange type
        with/without auto-delete and durable features.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        auto_delete_queue_values = [True, False]
        durable_queue_values = [True, False]
        exchange_types = ["direct", "topic", "fanout"]

        default_args = [
            "--pmessages",
            "1",
            "--consumers",
            "0",
        ]

        test_count = 1
        for t in exchange_types:
            for ad in auto_delete_queue_values:
                for d in durable_queue_values:
                    xname = "test_exchange_type_{}".format(t)
                    qname = "test_queue_for_ex_{}_ad_{}_d_{}".format(xname, ad, d)

                    base_args = [
                        "--queue",
                        qname,
                        "--exchange",
                        xname,
                        "--type",
                        t,
                    ]

                    extra_args = []

                    if ad is False:
                        # If auto-delete is NOT wanted then, pass this flag.
                        # This follows the perfTest spec of auto-delete=True by default.
                        extra_args.extend(["--auto-delete", "false"])
                    if d:
                        extra_args.extend(["--flag", "persistent"])

                    final_args = default_args + base_args + extra_args
                    print(
                        f"#### Starting test {test_count} with args: {final_args} ####"
                    )
                    self.start_perftest_executable(final_args)

                    assert self.wait_for_n_published(qname, 1)

                    queue_info = self.get_queue_info(qname)
                    print(
                        f"#### Checking queue for test {test_count} | {queue_info} ####"
                    )
                    assert queue_info["name"] == qname
                    assert queue_info["auto_delete"] == ad
                    # perfTest only enables durable queues when persistent messages are used,
                    # which makes sense.
                    # So, it is derived from the `--flag persistent` argument instead of --queue-args
                    assert queue_info["durable"] == d
                    # TODO: perfTest does not allow for exclusive queues, so this should always be false
                    assert queue_info["exclusive"] == False

                    exchange_info = self.get_exchange_info(xname)
                    print(
                        f"#### Checking exchange for test {test_count} | {exchange_info} ####"
                    )
                    assert exchange_info["name"] == xname
                    assert exchange_info["type"] == t

                    # TODO: perfTest does not allow specifying these params on an exchange
                    # It follows that these should be static
                    assert exchange_info["durable"] == True
                    assert exchange_info["auto_delete"] == False
                    assert exchange_info["internal"] == False

                    test_count += 1
