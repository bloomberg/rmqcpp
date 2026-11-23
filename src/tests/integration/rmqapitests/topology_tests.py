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
from __future__ import annotations

import logging

import pytest

from .integration_test import RmqTest

log = logging.getLogger(__name__)

pytestmark = pytest.mark.capability("topology")


class TestTopology(RmqTest):
    @pytest.mark.parametrize(
        "auto_delete", [True, False], ids=["auto-delete", "no-auto-delete"]
    )
    @pytest.mark.parametrize("exchange_type", ["direct", "topic", "fanout"])
    def test_topology(self, exchange_type, auto_delete):
        """
        Test that each standard exchange type can be declared, and that a queue
        can be declared and bound to it with and without auto-delete.

        An auto-delete queue is transient and any other queue is durable.
        perfTest only declares a durable queue when publishing persistent
        messages, so the durable case also passes `--flag persistent`.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "topology-queue"
        xname = "topology-exchange"
        durable = not auto_delete

        args = [
            "--pmessages",
            "1",
            "--producers",
            "1",
            "--consumers",
            "0",
            "--queue",
            qname,
            "--exchange",
            xname,
            "--type",
            exchange_type,
            "--auto-delete",
            "true" if auto_delete else "false",
        ]
        if durable:
            args.extend(["--flag", "persistent"])

        self.start_perftest_executable(args)

        assert self.wait_for_n_published(qname, 1)

        queue_info = self.get_queue_info(qname)
        log.info("queue info: %s", queue_info)
        assert queue_info["name"] == qname
        assert queue_info["auto_delete"] == auto_delete
        assert queue_info["durable"] == durable
        assert queue_info["exclusive"] is False

        exchange_info = self.get_exchange_info(xname)
        log.info("exchange info: %s", exchange_info)
        assert exchange_info["name"] == xname
        assert exchange_info["type"] == exchange_type
        assert exchange_info["durable"] is True
        assert exchange_info["auto_delete"] is False
        assert exchange_info["internal"] is False
