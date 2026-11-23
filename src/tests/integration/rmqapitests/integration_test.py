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
import os
import socket
import subprocess
import sys
import threading
from contextlib import suppress
from dataclasses import dataclass
from time import sleep
from typing import TYPE_CHECKING, Any

from .fixtures import Api, TestConfig

if TYPE_CHECKING:
    from .platforms import Platform

logger = logging.getLogger(__name__)
# Top level, so the adapter's lines read as `adapter` rather than trailing
# this module's dotted path.
adapter_logger = logging.getLogger("adapter")


def wait_or_kill(proc: subprocess.Popen, timeout: float = 5.0) -> None:
    """Wait for a subprocess to exit; send SIGKILL if it exceeds timeout."""
    for _ in range(int(timeout * 10)):
        if proc.poll() is not None:
            return
        sleep(0.1)
    proc.kill()
    proc.poll()


def _forward_adapter_output(stream: Any) -> None:
    """Re-emit each line of an adapter's output as it arrives.

    Runs on its own thread for the life of the pipe. Going through ``logging``
    puts the line in pytest's live log, tagged with the running test, so one
    grep spans both processes without the adapter doing anything.
    """
    for raw_line in iter(stream.readline, b""):
        line = raw_line.decode(errors="replace").rstrip()
        # Adapters that separate records with a blank line (BALL's default
        # observer format leads with a newline) would otherwise emit one
        # tagged-but-empty line per record.
        if not line:
            continue
        adapter_logger.info("%s", line)
    stream.close()


@dataclass
class QueueCounts:
    ready: Any
    unacknowledged: Any
    total: Any


class RmqTest:
    executables: list[subprocess.Popen[bytes]]
    _adapter_log_threads: list[threading.Thread]
    platform: Platform
    config: TestConfig
    api: Api
    specialArgs: list[Any]
    vhost: str
    prox: Any

    def setup_method(self, method):
        self.executables = []
        self.specialArgs = []
        self._adapter_log_threads = []

    def teardown_method(self, method):
        for p in self.executables:
            if p.returncode is None:
                p.terminate()
        for p in self.executables:
            if p.returncode is not None:
                continue
            wait_or_kill(p)
        # Let the readers drain the adapter's parting output into this test,
        # rather than the next one.
        for t in self._adapter_log_threads:
            t.join(timeout=5.0)
        self.specialArgs = []

    def start_perftest_executable(
        self,
        parameters: list[str],
        env_remove: list[str] | None = None,
        env_override: dict[str, str] | None = None,
    ) -> subprocess.Popen[bytes]:
        args = [self.config.exe]
        args.extend(parameters)
        args.extend(self.platform.connection_args(self.vhost))
        log_level = getattr(self, "adapter_log_level", None)
        if log_level:
            args.extend(["--log-level", log_level])

        logger.info("launching adapter: %s", " ".join(args))

        env = self.platform.adapter_env(dict(os.environ))
        if env_remove:
            for key in env_remove:
                env.pop(key, None)
        if env_override:
            env.update(env_override)

        # One pipe for both streams, so the adapter's own ordering survives.
        p = subprocess.Popen(
            args, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        self.executables.append(p)
        reader = threading.Thread(
            target=_forward_adapter_output, args=(p.stdout,), daemon=True
        )
        reader.start()
        self._adapter_log_threads.append(reader)
        return p

    def start_consumer(self, queue):
        return self.start_executable(exe=self.config.consumer_exe, queue=queue)

    def linux(self) -> bool:
        return sys.platform.startswith("linux")

    def runnable(self, executable: str, *kargs: Any, **kwargs: Any) -> tuple[int, str]:
        return subprocess.getstatusoutput(executable)

    def timed_expect_stable(self, expression, duration=5, step=0.5) -> bool:
        """True if ``expression`` holds for the whole of ``duration``.

        Some assertions are about nothing happening -- a consumer staying
        paused, rather than being paused at the instant we look.
        """
        waited = 0.0
        while waited < duration:
            if not expression():
                return False
            sleep(step)
            waited += step
        return True

    def timed_expect_true(self, expression, timeout=10, step=0.5) -> bool:
        waited = 0.0

        seen_exception = None

        while True:
            try:
                if expression():
                    return True
            except Exception as e:  # noqa: BLE001 — test helper retries on any error
                seen_exception = e
                logger.info(
                    f"Exception raised in timed_expect_true. Continuing in case it does eventually succeed: {e}"
                )
            if waited > timeout:
                if seen_exception:
                    # Raise a previously seen exception if we've seen one
                    raise seen_exception

                return False
            sleep(step)
            waited += step

    def get_connections(self) -> Any:
        connections = self.api.get(f"/api/vhosts/{self.vhost}/connections").json()
        return list(connections)

    def get_connection_count(self) -> int:
        return len(self.get_connections())

    def close_all_connections(self) -> int:
        connections = self.get_connections()
        for connection in connections:
            logger.info(f"gracefully closing {connection['name']}")
            self.api.delete(f"/api/connections/{connection['name']}")
        return len(connections)

    def wait_for_connection(self, timeout=60) -> bool:
        return self.timed_expect_true(
            lambda: self.get_connection_count() > 0, timeout, 0.5
        )

    def get_channel_count(self) -> int:
        channels = self.api.get(f"/api/vhosts/{self.vhost}/channels").json()
        return len(channels)

    def exist_consumer(self, consumer_tag, qname) -> bool:
        consumers = self.api.get(f"/api/consumers/{self.vhost}").json()
        for consumer in consumers:
            try:
                if (
                    consumer["consumer_tag"] == consumer_tag
                    and consumer["queue"]["name"] == qname
                ):
                    return True
            except KeyError:
                pass
        return False

    def exist_n_consumers(self, qname, num_consumers) -> bool:
        return self.get_consumer_count(qname) == num_consumers

    def exist_priority_consumer(self, qname, priority) -> bool:
        queue_details = self.api.get(f"/api/queues/{self.vhost}/{qname}").json()
        for consumer in queue_details["consumer_details"]:
            logger.info(
                f"priority found: {consumer['arguments'].get('x-priority')} vs testing {priority}"
            )
            if consumer["arguments"].get("x-priority") == priority:
                return True
        return False

    def wait_for_n_consumers(self, queue, num_consumers, timeout=60):
        return self.timed_expect_true(
            lambda: self.exist_n_consumers(queue, num_consumers), timeout, 0.5
        )

    def wait_for_consumer(self, consumer_tag, qname, timeout=60) -> bool:
        return self.timed_expect_true(
            lambda: self.exist_consumer(consumer_tag, qname), timeout, 0.5
        )

    def wait_for_n_published(
        self, queue, n_published, zero_if_not_exists=False, timeout=60
    ) -> bool:
        return self.timed_expect_true(
            lambda: self.get_message_count(queue, zero_if_not_exists) == n_published,
            timeout,
            0.5,
        )

    def get_message_count(self, qname, zero_if_not_exists=False) -> Any:
        try:
            queue = self.api.get(
                f"/api/queues/{self.vhost}/{qname}/",
                not_found_allowed=zero_if_not_exists,
            ).json()
        except Exception:  # noqa: BLE001 — best-effort test helper
            return None
        if "messages" in queue:
            return queue["messages"]
        elif zero_if_not_exists and queue.get("error") == "Object Not Found":
            return 0
        else:
            return None

    def get_queue_counts(self, qname) -> QueueCounts:
        """One Management API read, so the three numbers describe the same
        instant.
        """
        queue = self.get_queue_info(qname)
        if queue is None:
            return QueueCounts(None, None, None)
        return QueueCounts(
            ready=queue.get("messages_ready"),
            unacknowledged=queue.get("messages_unacknowledged"),
            total=queue.get("messages"),
        )

    def get_ready_message_count(self, qname) -> Any:
        return self.get_queue_counts(qname).ready

    def get_unacked_message_count(self, qname) -> Any:
        return self.get_queue_counts(qname).unacknowledged

    def get_consumer_count(self, qname) -> Any:
        """How many consumers the broker has registered on a queue.

        ``None`` before the broker's first statistics collection, when the API
        returns a reduced object whose empty ``consumer_details`` is
        indistinguishable from a consumer that was just cancelled.
        """
        queue = self.get_queue_info(qname)
        if queue is None or "consumers" not in queue:
            return None
        return len(queue.get("consumer_details", []))

    def get_consumer_prefetches(self, qname) -> list[int]:
        """The prefetch count the broker has recorded for each consumer.

        A consumer whose prefetch was not restored after a reconnect reports
        0, meaning unlimited.
        """
        queue = self.get_queue_info(qname)
        if queue is None or "consumers" not in queue:
            return []
        return [c.get("prefetch_count") for c in queue.get("consumer_details", [])]

    def publish_message(self, message) -> Any:
        return self.api.post(
            f"/api/exchanges/{self.vhost}/amq.default/publish", json=message
        ).json()

    def wait_for_message_consumption(
        self, qname, count=0, timeout=60, success_on_queue_delete=False
    ) -> bool:
        step = 0.2
        for _ in range(int(timeout / step)):
            if (
                self.get_message_count(
                    qname, zero_if_not_exists=success_on_queue_delete
                )
                == count
            ):
                return True
            else:
                sleep(step)
        return False

    def delete_queue_if_exists(self, qname) -> Any:
        with suppress(Exception):
            self.api.delete(f"/api/queues/{self.vhost}/{qname}")
            self.timed_expect_true(lambda: self.get_message_count(qname) is None)

    def get_exchange_info(self, xname) -> Any:
        try:
            return self.api.get(f"/api/exchanges/{self.vhost}/{xname}/").json()
        except Exception:  # noqa: BLE001 — best-effort test helper
            return None

    def get_queue_info(self, qname) -> Any:
        try:
            return self.api.get(f"/api/queues/{self.vhost}/{qname}/").json()
        except Exception:  # noqa: BLE001 — best-effort test helper
            return None

    def create_queue(self, qname) -> Any:
        try:
            return self.api.put(
                f"/api/queues/{self.vhost}/{qname}/", json={"durable": True}
            ).json()
        except Exception:  # noqa: BLE001 — best-effort test helper
            return None

    def purge_queue(self, qname) -> Any:
        try:
            return self.api.delete(f"/api/queues/{self.vhost}/{qname}/contents").json()
        except Exception:  # noqa: BLE001 — best-effort test helper
            return None


class SpoofHungServer:
    """Class to simulate a permanently or temporarily hung port"""

    def __init__(self, port: int = 0) -> None:
        self._hung_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._hung_server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._hung_server_socket.bind(("0.0.0.0", port))
        self._hung_server_socket.listen(1)
        self._dest_socket: socket.socket | None = None
        self._client_socket: socket.socket | None = None
        self._terminate = False
        self._forwarding = False
        self._listening_thread: threading.Thread | None = None
        self._forward_thread: threading.Thread | None = None
        self._server_hung = False
        self._spoof_conn: socket.socket | None = None
        self._spoof_addr: tuple[str, int] | None = None

    def __del__(self) -> None:
        self._terminate = True

    @property
    def spoof_port(self) -> int:
        return self._hung_server_socket.getsockname()[1]

    def assert_spoofed(self) -> None:
        """assert the spoofed port has accepted a connection"""
        if not self._server_hung:
            raise RuntimeError("Must start_hung_server before testing if spoof worked")
        assert isinstance(self._spoof_conn, socket.socket)
        logger.info("Confirmed connection to hung port.")
        self._server_hung = False

    def hung_serve(self) -> None:
        self._server_hung = True
        while self._server_hung:
            try:
                self._spoof_conn, self._spoof_addr = self._hung_server_socket.accept()
            except OSError:
                break
            logger.info(
                f"Connection from {self._spoof_addr[0]}:{self._spoof_addr[1]} hit hung socket on localhost:{self.spoof_port}."
            )

    def start_hung_server(self) -> None:
        threading.Thread(target=self.hung_serve, daemon=True).start()

    def start_forwarding(self, dest_host: str, dest_port: int) -> None:
        """Accepts a connection on the instantiated spoofed port and will transfer packets between
        the connected client and the supplied destination host and port.
        """
        self._server_hung = False

        self._client_socket, _addr = self._hung_server_socket.accept()

        self._dest_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._dest_socket.connect((dest_host, dest_port))

        logger.debug(f"forwarding {self._client_socket}->{self._dest_socket}")
        self._listening_thread = threading.Thread(
            target=self.forward,
            args=[self._client_socket, self._dest_socket],
            daemon=True,
        )
        self._listening_thread.start()

        logger.debug(f"forwarding {self._client_socket}<-{self._dest_socket}")
        self._forward_thread = threading.Thread(
            target=self.forward,
            args=[self._dest_socket, self._client_socket],
            daemon=True,
        )
        self._forward_thread.start()
        self._forwarding = True

    def forward(self, source: socket.socket, dest: socket.socket) -> None:
        while not self._terminate:
            data = source.recv(1024)
            if data:
                logger.debug(f"sending data to {source} from {dest}")
                dest.sendall(data)

    def stop(self) -> None:
        self._terminate = True
        self._server_hung = False
        try:
            self._hung_server_socket.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self._hung_server_socket.close()
        if self._forwarding:
            while self._listening_thread.is_alive() or self._forward_thread.is_alive():  # type: ignore[union-attr]
                logger.info("Waiting for threads to terminate")
                sleep(5)
            try:
                self._client_socket.shutdown(socket.SHUT_RDWR)  # type: ignore[union-attr]
            except OSError:
                pass
            try:
                self._dest_socket.shutdown(socket.SHUT_RDWR)  # type: ignore[union-attr]
            except OSError:
                pass
