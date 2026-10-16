"""Copyright 2026 Bloomberg Finance L.P.
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

from .integration_test import RmqTest, wait_or_kill

log = logging.getLogger(__name__)

pytestmark = pytest.mark.capability("hosthealthawareness")


HEALTHY = True
UNHEALTHY = False

# The timeout covers a poll interval plus the Management API's reporting lag.
HOST_HEALTH_POLL_INTERVAL = 1
HOST_HEALTH_TRANSITION_TIMEOUT = 30

HOST_HEALTH_QOS = 10
HOST_HEALTH_TOTAL_MESSAGES = 60

# Deliberately slow: the backlog has to outlast the Management API's reporting
# lag on both sides of a transition, or the queue empties and a consumer that
# never paused looks just like one that did.
HOST_HEALTH_LATENCY_US = 400_000

# Slower still for the prefetch test, so draining the in-flight window and
# requeueing it are visibly different.
HOST_HEALTH_DRAIN_LATENCY_US = 800_000
HOST_HEALTH_DRAIN_TOTAL_MESSAGES = 30

# Several poll intervals, so a consumer that is going to misbehave has had the
# chance to.
HOST_HEALTH_STABLE_WINDOW = 5


class _HostHealthAware:
    """Gives a test a host health signal it can flip."""

    def setup_method(self, method):
        super().setup_method(method)
        self._host_health = self.platform.host_health_driver()
        self._host_health.setup(self.vhost)

    def teardown_method(self, method):
        # After the base class, so the adapter is gone before the signal it was
        # reading is taken away.
        super().teardown_method(method)
        self._host_health.teardown()

    def set_host_health(self, healthy: bool) -> None:
        self._host_health.set_health(healthy)

    def host_health_args(
        self, poll_interval: float = HOST_HEALTH_POLL_INTERVAL
    ) -> list[str]:
        """Adapter arguments making it host health aware for this test.

        Passing a poll interval is what asks for host health awareness at all;
        omitting these arguments entirely gives a context without it.
        """
        return ["--host-health-poll-interval", str(poll_interval)]


class _HostHealthTest(_HostHealthAware, RmqTest):
    """Publish a backlog, start a consumer against a known health state, flip
    the state, watch the broker. Every test here has that shape.
    """

    def publish_backlog(self, qname, count=HOST_HEALTH_TOTAL_MESSAGES):
        """Fill a queue with more work than the consumer can finish at once."""
        self.create_queue(qname)
        for i in range(count):
            response = self.publish_message(
                {
                    "properties": {},
                    "routing_key": qname,
                    "payload": f"message-{i}",
                    "payload_encoding": "string",
                }
            )
            assert response["routed"] is True
        assert self.wait_for_n_published(qname, count)
        log.info("published %d messages to %s", count, qname)

    def start_health_consumer(
        self,
        qname,
        qos=HOST_HEALTH_QOS,
        latency_us=HOST_HEALTH_LATENCY_US,
        health_aware_context=True,
    ):
        """Start one consumer, host health aware unless told otherwise.

        ``health_aware_context=False`` gives a context with no health config.
        """
        args = [
            "--queue",
            qname,
            "--consumers",
            "1",
            "--producers",
            "0",
            "--qos",
            str(qos),
        ]
        if latency_us is not None:
            args += ["--consumer-latency", str(latency_us)]
        if health_aware_context:
            args += self.host_health_args()

        proc = self.start_perftest_executable(args)
        if not self.wait_for_connection():
            pytest.fail("Failed due to no connection being established")
        return proc

    def assert_consuming(self, qname, timeout=HOST_HEALTH_TRANSITION_TIMEOUT):
        """The consumer is registered on the queue."""
        assert self.timed_expect_true(
            lambda: self.get_consumer_count(qname) == 1, timeout=timeout
        ), f"consumer never registered on {qname}"

    def assert_paused(self, qname, timeout=HOST_HEALTH_TRANSITION_TIMEOUT):
        """The consumer is cancelled at the broker but still connected."""
        channels_before = self.get_channel_count()
        assert self.timed_expect_true(
            lambda: self.get_consumer_count(qname) == 0, timeout=timeout
        ), f"consumer on {qname} did not pause"
        assert self.get_connection_count() > 0, (
            "pausing dropped the connection; it must stay open so that "
            "prefetched messages can still be acknowledged"
        )
        assert self.get_channel_count() == channels_before, (
            "pausing closed the consumer's channel"
        )

    def assert_stays_paused(self, qname, duration=HOST_HEALTH_STABLE_WINDOW):
        """No consumer appears on the queue for the whole window.

        Strictly zero, not ``None`` -- a queue with no published statistics
        reports no consumers whether or not one is attached.
        """
        assert self.timed_expect_stable(
            lambda: self.get_consumer_count(qname) == 0, duration=duration
        ), f"a consumer appeared on {qname} while the host was down"

    def wait_for_prefetch_drain(self, qname, timeout=30):
        """Wait for the in-flight window to be acknowledged after a pause."""
        assert self.timed_expect_true(
            lambda: self.get_unacked_message_count(qname) == 0, timeout=timeout
        ), (
            "prefetched messages were never acknowledged after the pause; "
            "the consumer must finish work it has already been given"
        )

    def wait_for_progress(self, qname, at_least=10, total=HOST_HEALTH_TOTAL_MESSAGES):
        """Wait until the consumer has visibly worked through some backlog."""
        assert self.timed_expect_true(
            lambda: self.get_message_count(qname) <= total - at_least, timeout=60
        ), f"consumer did not get through {at_least} messages"


class TestConsumerHostHealthStartup(_HostHealthTest):
    """How a consumer is created, before any health transition happens."""

    def test_consumes_when_host_healthy_at_startup(self):
        """On a healthy host a health-aware consumer behaves like any other."""
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "host-health-startup-up-queue"

        self.set_host_health(HEALTHY)
        self.publish_backlog(qname)
        self.start_health_consumer(qname, latency_us=None)

        self.assert_consuming(qname)
        assert self.wait_for_message_consumption(
            qname, timeout=180, success_on_queue_delete=True
        )

    def test_starts_paused_when_host_unhealthy_at_startup(self):
        """A consumer created on a down host must never subscribe at all.

        Not subscribe and back out: at a realistic poll interval that would
        pull a whole prefetch window onto a machine being drained.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "host-health-startup-down-queue"

        self.set_host_health(UNHEALTHY)
        self.publish_backlog(qname)
        self.start_health_consumer(qname)

        self.assert_stays_paused(qname)
        assert self.get_message_count(qname) == HOST_HEALTH_TOTAL_MESSAGES, (
            "messages were consumed while the host was down"
        )
        assert self.get_connection_count() > 0, (
            "the adapter should be connected and waiting, not disconnected"
        )

        # And it is waiting on host health rather than simply broken.
        self.set_host_health(HEALTHY)
        self.assert_consuming(qname)
        assert self.wait_for_message_consumption(
            qname, timeout=180, success_on_queue_delete=True
        )


class TestConsumerHostHealthTransitions(_HostHealthTest):
    """Pausing and resuming as the host health changes under a live consumer."""

    def test_pauses_when_host_goes_down(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "host-health-down-queue"

        self.set_host_health(HEALTHY)
        self.publish_backlog(qname)
        self.start_health_consumer(qname)
        self.assert_consuming(qname)
        self.wait_for_progress(qname)

        self.set_host_health(UNHEALTHY)
        self.assert_paused(qname)
        self.wait_for_prefetch_drain(qname)

        ready = self.get_ready_message_count(qname)
        assert ready > 0, (
            "the backlog was fully drained before the pause landed; "
            "this run proves nothing about pausing"
        )
        assert self.timed_expect_stable(
            lambda: self.get_ready_message_count(qname) == ready,
            duration=HOST_HEALTH_STABLE_WINDOW,
        ), "the consumer kept taking new messages while the host was down"

    def test_resumes_when_host_comes_back_up(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "host-health-up-queue"

        self.set_host_health(HEALTHY)
        self.publish_backlog(qname)
        self.start_health_consumer(qname)
        self.assert_consuming(qname)
        self.wait_for_progress(qname)

        self.set_host_health(UNHEALTHY)
        self.assert_paused(qname)
        self.wait_for_prefetch_drain(qname)
        remaining = self.get_ready_message_count(qname)
        assert remaining > 0

        self.set_host_health(HEALTHY)
        self.assert_consuming(qname)
        assert self.wait_for_message_consumption(
            qname, timeout=180, success_on_queue_delete=True
        ), f"consumer did not finish the {remaining} messages left after resuming"

    def test_tracks_repeated_health_flapping(self):
        """A library that reacts only to the first change, or latches after one
        cycle, passes the single pause and resume tests and fails here.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "host-health-flapping-queue"

        self.set_host_health(HEALTHY)
        self.publish_backlog(qname)
        self.start_health_consumer(qname)
        self.assert_consuming(qname)

        for cycle in range(2):
            log.info("flap cycle %d: taking the host down", cycle + 1)
            self.set_host_health(UNHEALTHY)
            self.assert_paused(qname)

            log.info("flap cycle %d: bringing the host back up", cycle + 1)
            paused_at = self.get_message_count(qname)
            self.set_host_health(HEALTHY)
            self.assert_consuming(qname)
            self.wait_for_progress(qname, at_least=1, total=paused_at)

        assert self.wait_for_message_consumption(
            qname, timeout=180, success_on_queue_delete=True
        ), "consumer did not drain the queue after flapping"


class TestConsumerHostHealthPrefetch(_HostHealthTest):
    """What happens to messages already delivered when the pause lands."""

    def test_prefetched_messages_are_acknowledged_not_requeued(self):
        """The in-flight window is worked through, not thrown back.

        Acknowledging drains messages_unacknowledged to zero and leaves
        messages_ready alone; requeueing pushes messages_ready up.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "host-health-prefetch-queue"

        self.set_host_health(HEALTHY)
        self.publish_backlog(qname, HOST_HEALTH_DRAIN_TOTAL_MESSAGES)
        self.start_health_consumer(qname, latency_us=HOST_HEALTH_DRAIN_LATENCY_US)
        self.assert_consuming(qname)

        # Precondition: the pause has to land while a full window is in flight,
        # or there is nothing for the consumer to work through.
        assert self.timed_expect_true(
            lambda: self.get_unacked_message_count(qname) == HOST_HEALTH_QOS,
            timeout=30,
        ), "consumer never held a full prefetch window"

        self.set_host_health(UNHEALTHY)
        self.assert_paused(qname)

        at_pause = self.get_queue_counts(qname)
        log.info(
            "paused with %s ready and %s unacknowledged",
            at_pause.ready,
            at_pause.unacknowledged,
        )
        assert at_pause.unacknowledged > 0, (
            "the window had already emptied when the pause completed; "
            "this run proves nothing about the drain"
        )

        self.wait_for_prefetch_drain(qname)
        after_drain = self.get_queue_counts(qname)

        # Requeueing shows up as messages_ready *growing*. It is <= rather
        # than == because at_pause.ready is a lagged read taken slightly before
        # the cancel lands, so a message or two can still arrive in between.
        assert after_drain.ready <= at_pause.ready, (
            f"in-flight messages were requeued rather than acknowledged: "
            f"messages_ready grew {at_pause.ready} -> {after_drain.ready}"
        )
        assert after_drain.total == after_drain.ready, (
            f"messages left unacknowledged after the drain: "
            f"{after_drain.total} total against {after_drain.ready} ready"
        )

        assert self.timed_expect_stable(
            lambda: self.get_ready_message_count(qname) == after_drain.ready,
            duration=HOST_HEALTH_STABLE_WINDOW,
        ), "the consumer kept taking new messages while the host was down"

        self.set_host_health(HEALTHY)
        self.assert_consuming(qname)
        assert self.wait_for_message_consumption(
            qname, timeout=180, success_on_queue_delete=True
        )


class TestConsumerHostHealthReconnection(_HostHealthTest):
    """Paused state has to survive the connection dropping underneath it."""

    def _pause_then_disconnect(self, qname, disconnect):
        """Pause, drop the connection, confirm it recovers paused. Returns the
        ready count, which must not have moved.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)

        self.set_host_health(HEALTHY)
        self.publish_backlog(qname)
        self.start_health_consumer(qname)
        self.assert_consuming(qname)
        self.wait_for_progress(qname)

        self.set_host_health(UNHEALTHY)
        self.assert_paused(qname)
        # Let the in-flight window finish before pulling the connection.
        # Anything still unacknowledged would be redelivered by the broker and
        # muddy the before/after comparison.
        self.wait_for_prefetch_drain(qname)
        ready_before = self.get_ready_message_count(qname)
        assert ready_before > 0

        log.info("dropping the connection with the consumer paused")
        disconnect()
        assert self.timed_expect_true(
            lambda: self.get_connection_count() == 0, timeout=15
        ), "connection did not drop"
        assert self.wait_for_connection(timeout=60), (
            "adapter did not reconnect after the connection was dropped"
        )

        # Recovered, but still paused: the channel is back and QoS re-applied,
        # yet no consumer is registered and nothing is being taken.
        self.assert_stays_paused(qname)
        assert self.get_ready_message_count(qname) == ready_before, (
            "the consumer resumed on reconnection while the host was still down"
        )
        return ready_before

    def test_stays_paused_after_graceful_disconnect(self):
        self._pause_then_disconnect(
            "host-health-graceful-queue", self.close_all_connections
        )

    def test_stays_paused_after_forceful_disconnect(self):
        self._pause_then_disconnect(
            "host-health-forceful-queue", self.prox.force_close_connections
        )

    def test_resumes_after_reconnecting_while_paused(self):
        """The recovered consumer has to be resumable, not merely quiet -- a
        library that loses track of it looks identical until the host is back.
        """
        qname = "host-health-reconnect-resume-queue"
        self._pause_then_disconnect(qname, self.close_all_connections)

        self.set_host_health(HEALTHY)
        self.assert_consuming(qname)
        assert self.wait_for_message_consumption(
            qname, timeout=180, success_on_queue_delete=True
        ), "consumer did not drain the queue after recovering and resuming"


class TestConsumerHostHealthUnconfigured(_HostHealthTest):
    """A context that is never told to be host health aware at all."""

    def test_context_without_health_config_consumes_while_host_down(self):
        """The host is driven down, but the adapter is deliberately not told to
        be health aware, so a library that reads host health unconfigured fails
        here.
        """
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "host-health-unconfigured-queue"

        self.set_host_health(UNHEALTHY)
        self.publish_backlog(qname)
        self.start_health_consumer(qname, latency_us=None, health_aware_context=False)

        self.assert_consuming(qname)
        assert self.wait_for_message_consumption(
            qname, timeout=180, success_on_queue_delete=True
        )


class TestProducerUnaffectedByHostHealth(_HostHealthAware, RmqTest):
    """Host health awareness pauses consumers only, never producers.

    A library that pauses whole connections rather than consumer subscriptions
    fails here.
    """

    def test_produces_while_host_unhealthy(self):
        assert self.timed_expect_true(lambda: self.get_connection_count() == 0)
        qname = "host-health-producer-queue"
        exch = "my-exchange"
        N = 10

        self.set_host_health(UNHEALTHY)

        p = self.start_perftest_executable(
            [
                "--exchange",
                exch,
                "--queue",
                qname,
                "--pmessages",
                str(N),
                "--consumers",
                "0",
                "--producers",
                "1",
                "--routing-key",
                "routing-key",
                *self.host_health_args(),
            ]
        )

        assert self.wait_for_n_published(qname, N), (
            "producer did not publish while the host was unhealthy"
        )

        wait_or_kill(p, timeout=10.0)
        assert p.returncode == 0
