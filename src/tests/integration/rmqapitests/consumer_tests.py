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
import time

import pytest

from .integration_test import RmqTest

log = logging.getLogger(__name__)

pytestmark = pytest.mark.capability("consumer")

MESSAGE_COUNT = 100


class TestCancelConsumeMessage(RmqTest):
    def test_cancel_consume(self):
        """
        Confirm that a single consumer can process exactly 50 messages and then stop.

        Confirm 10 messages are left over.

        Passes:
        --auto-delete false
        --queue cancel-consumer-queue
        --consumers 1
        --cmessages 50
        --producers 0
        --uri <amqp endpoint>
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "cancel-consumer-queue"
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
        log.info("Starting producer.")
        for _ in range(60):
            response = self.publish_message(message)
            assert response["routed"] is True

        if not self.wait_for_message_consumption(
            qname, 10, success_on_queue_delete=True
        ):
            pytest.fail("Consumer is not able to consume the message.")


class TestProduceConsumeMessages(RmqTest):
    def test_cancel_consume_after_n_messages(self):
        """
        Confirm that a consumer stops after 50 messages while a producer in the
        same executable publishes 100.
        """
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
                "--producers",
                "1",
                "--consumers",
                "1",
            ]
        )

        if not self.wait_for_message_consumption(
            qname, 50, success_on_queue_delete=True
        ):
            pytest.fail("Consumer is not able to consume the message.")


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

        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        assert self.wait_for_n_published(qname, N)

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

        assert self.timed_expect_true(
            lambda: self.get_connection_count() == 0, timeout=30
        )

        if not self.wait_for_message_consumption(qname, success_on_queue_delete=True):
            pytest.fail(f"Consumer is not able to consume the messages from {qname}")
        self.delete_queue_if_exists(qname)


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
        log.info(f"{consumer_queue_names}")
        self.start_perftest_executable(
            [
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
            log.info(f"waiting for a consumer on {queue}")
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
        qname = "create-consumer-queue"
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
    --queue create-consumer-queue
    --consumers 1
    --consumer-args  "x-priority=0"
    --cmessages 1
    --producers 0
    --uri <amqp endpoint>

    And:
    --queue create-consumer-queue
    --consumers 1
    --consumer-args  "x-priority=1"
    --cmessages 5
    --producers 0
    --uri <amqp endpoint>

    """

    @pytest.mark.parametrize(
        ("low_priority", "high_priority"),
        [
            (0, 1),
            (-2, 1),
            (-255, 255),
            (2**63 - 2, 2**63 - 1),
            (-(2**63) + 1, -(2**63) + 2),
        ],
        ids=[
            "zero-vs-positive-priority",
            "negative-vs-positive-priority",
            "wide-symmetric-priority-range",
            "int64-max-priority-boundary",
            "int64-min-priority-boundary",
        ],
    )
    def test_consume(self, low_priority, high_priority):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "create-consumer-queue"
        num_messages = 5

        self.start_perftest_executable(
            [
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
            ]
        )
        self.start_perftest_executable(
            [
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
            ]
        )
        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")

        assert self.wait_for_n_consumers(qname, 2)

        for _ in range(num_messages):
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
        log.info("testing high priority consumer is gone")
        assert self.timed_expect_true(
            lambda: not self.exist_priority_consumer(qname, high_priority), timeout=20
        )
        log.info("testing low priority consumer still exists")
        # low priority consumer should not be destructed because it should not have consumed a message
        assert self.exist_priority_consumer(qname, low_priority)


class TestConsumerPauseResume(RmqTest):
    def _start_pausing_consumer(self, qname, qos, pause_after, resume_after):
        """Start a consumer that pauses after ``pause_after`` messages."""
        log.info(f"Starting consumer that will pause after {pause_after} messages")
        started = time.monotonic()
        self.start_perftest_executable(
            [
                "--queue",
                qname,
                "--consumers",
                "1",
                "--qos",
                str(qos),
                "--pause-after-messages",
                str(pause_after),
                "--resume-after-seconds",
                str(resume_after),
                "--producers",
                "0",
            ]
        )

        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")

        return started

    def _assert_paused_midway(self, qname, resume_after):
        """Check the consumer has stopped, looking once from mid-pause.

        Watching for the moment it stops means racing the management
        statistics, which trail the broker by seconds. Looking once, well
        inside the pause, does not: a consumer that paused has left the rest
        of the queue alone, and one that ignored the instruction has emptied
        it. The reading only has to tell those two apart.
        """
        time.sleep(resume_after / 2)
        left = self.get_message_count(qname)
        if left is None:
            # One retry, so a management call that failed does not read as an
            # empty queue and get reported as a consumer that never stopped.
            left = self.get_message_count(qname)

        assert left, (
            f"halfway through the pause the queue read {left}, so either the "
            f"consumer never stopped or the broker could not be reached"
        )
        log.info(f"Queue still holds {left} messages")

    def _assert_drained_no_sooner_than_the_pause(
        self, qname, started, resume_after, timeout=60
    ):
        """Check everything was consumed, and not before the pause was up.

        A consumer that never pauses finishes in about a second, so the two
        cases differ by an order of magnitude rather than by a margin. Slow
        statistics can only make this look longer than it was, never shorter,
        so they cannot fail it.

        ``started`` is taken before the consumer is launched, so the elapsed
        time includes its startup. That makes this check slightly lenient
        rather than slightly strict, which is the direction to err in.
        """
        if not self.wait_for_message_consumption(
            qname, count=0, timeout=timeout, success_on_queue_delete=True
        ):
            pytest.fail(
                f"Consumer did not consume all messages. "
                f"{self.get_message_count(qname)} remaining."
            )

        elapsed = time.monotonic() - started
        log.info(f"Queue drained after {elapsed:.1f}s")
        assert elapsed >= resume_after, (
            f"the queue drained in {elapsed:.1f}s, inside the {resume_after}s "
            f"pause the consumer was asked to take, so it did not pause"
        )

    def _publish_messages(self, qname, total_messages):
        log.info(f"Publishing {total_messages} messages")
        self.create_queue(qname)
        for i in range(total_messages):
            message = {
                "properties": {},
                "routing_key": qname,
                "payload": f"message-{i}",
                "payload_encoding": "string",
            }
            response = self.publish_message(message)
            assert response["routed"] is True

        log.info(f"All {total_messages} messages published")

    @pytest.mark.parametrize(
        ("qname", "qos", "pause_after", "total_messages", "resume_after"),
        [
            ("ordinary-queue", 5, 10, 50, 15),
            ("high-volume-queue", 10, 20, 500, 15),
            ("low-prefetch-queue", 1, 5, 100, 15),
        ],
        ids=[
            "ordinary-queue-qos5",
            "high-volume-queue-qos10",
            "low-prefetch-queue-qos1",
        ],
    )
    def test_pause_resume(self, qname, qos, pause_after, total_messages, resume_after):
        """
        This test verifies:
        1. Consumer can process messages normally
        2. Consumer can be paused after processing N messages
        3. When paused, consumer stops processing beyond prefetch (qos)
        4. Consumer can be resumed and continues processing messages

        Passes:
        --queue pause-resume-queue
        --consumers 1
        --qos 5
        --pause-after-messages pause_after
        --resume-after-seconds resume_after
        --producers 0
        --uri <amqp endpoint>
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)

        self._publish_messages(qname, total_messages)
        started = self._start_pausing_consumer(qname, qos, pause_after, resume_after)
        self._assert_paused_midway(qname, resume_after)
        self._assert_drained_no_sooner_than_the_pause(qname, started, resume_after)

        log.info("Test passed! Consumer paused, then resumed and consumed the rest.")

    def test_consumer_reconnects_in_paused_state_after_disconnection(self):
        """
        This test:
        1. Pauses consumer after N messages
        2. Closes the connection from broker side
        3. Waits for automatic reconnection
        4. Verifies consumer recovers in paused state
        5. Verifies consumer can resume and process remaining messages
        """

        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "pause-reconnect-queue"
        qos = 5  # prefetch count
        pause_after = 50  # pause after consuming 50 messages
        total_messages = 100
        resume_after = 15  # resume after 15s of being paused

        self._publish_messages(qname, total_messages)
        started = self._start_pausing_consumer(qname, qos, pause_after, resume_after)
        self._assert_paused_midway(qname, resume_after)

        log.info("Forcing connection close from broker side...")
        closed_count = self.close_all_connections()
        log.info(f"Closed {closed_count} connections")

        log.info("Waiting for disconnect to be detected...")
        assert self.timed_expect_true(
            lambda: self.get_connection_count() == 0, timeout=10
        ), "Connection should be closed"

        log.info("Waiting for automatic reconnection...")
        assert self.wait_for_connection(timeout=30), (
            "Connection should be re-established automatically"
        )

        # Coming back paused rather than treating the reconnect as a reason
        # to start again is covered by the drain check: a consumer that
        # resumed early finishes inside the pause it was asked to take.
        self._assert_drained_no_sooner_than_the_pause(
            qname, started, resume_after, timeout=90
        )

        log.info(
            "Test passed! Consumer paused, survived a forced disconnect, and "
            "resumed afterwards."
        )


class TestConsumerPrefetchSurvivesReconnect(RmqTest):
    """
    Confirm a consumer's prefetch survives a reconnect, since basic.qos is
    channel state rather than topology.

    Passes:
    --queue prefetch-reconnect-queue
    --consumers 1
    --qos 10
    --producers 0
    """

    QOS = 10

    def test_prefetch_survives_reconnect(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "prefetch-reconnect-queue"

        self.start_perftest_executable(
            [
                "--queue",
                qname,
                "--consumers",
                "1",
                "--qos",
                str(self.QOS),
                "--producers",
                "0",
            ]
        )
        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")
        assert self.wait_for_n_consumers(qname, 1)
        assert self.timed_expect_true(
            lambda: self.get_consumer_prefetches(qname) == [self.QOS]
        ), f"prefetch before reconnect: {self.get_consumer_prefetches(qname)}"

        self.prox.force_close_connections()
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        if not self.wait_for_connection():
            pytest.fail("Failed due to client not reconnecting")

        assert self.wait_for_n_consumers(qname, 1)
        assert self.timed_expect_true(
            lambda: self.get_consumer_prefetches(qname) == [self.QOS], 30
        ), (
            f"prefetch not restored after reconnect: asked for {self.QOS}, "
            f"broker reports {self.get_consumer_prefetches(qname)}"
        )
