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
import logging
import socket
import sys
import subprocess
import threading
import os
from time import sleep
from typing import List, Any, Optional
from .fixtures import TestConfig, TlsConfig

logging.basicConfig(stream=sys.stdout)


class RmqTest:
    executables: List[subprocess.Popen]
    config: TestConfig
    specialArgs: List[Any]

    def setup_method(self, method):
        self.executables = []
        self.specialArgs = []

    def teardown_method(self, method):
        for p in self.executables:
            p.terminate()
        self.specialArgs = []

    def start_perftest_executable(
        self, parameters: List[str], alt_exe: bool = False
    ) -> subprocess.Popen:
        """
        The main executor function which runs the test binary.
        The test executable and other args are set via ENV vars discovered when the
        TestConfig object is created.

        The `RMQ_EXE` var is set to the primary test runner.
        `RMQ_ALT_EXE` can be used if a secondary binary is needed.
        Call this function with `alt_exe=True` to use the ALT env var.

        Passed by default, any executable called by this function must accept:
          --uri  |  str: the full amqp endpoint `amqp://user:password@host:port/vhost`

        Arguments:
            parameters: List[str] - A list of args to pass to the test binary like ["--arg1", "val1",...]
            alt_exe: bool - Set to True if the binary set to env var 'RMQ_ALT_EXE' should be called instead
                            of the from the default 'RMQ_EXE'
        """
        protocol = "amqp"

        connection_string = f"{protocol}://{self.config.user}:{self.config.pwd}@{self.config.hostname}:{self.config.port}/{self.vhost}"

        exe = self.config.alt_exe if alt_exe else self.config.exe

        args = [exe]
        args.extend(parameters)
        args.extend(["--uri", connection_string])

        print("args: {}", args)

        env = dict(os.environ)
        env["PERF_TEST_LOG"] = "TRACE"

        p = subprocess.Popen(args, env=env)
        self.executables.append(p)
        return p

    def linux(self):
        return sys.platform.startswith("linux")

    def runnable(self, executable, *kargs, **kwargs) -> (int, str):
        return subprocess.getstatusoutput(executable)

    def timed_expect_true(self, expression, timeout=10, step=0.5) -> bool:
        waited = 0.0

        seen_exception = None

        while True:
            try:
                if expression():
                    return True
            except Exception as e:
                seen_exception = e
                print(
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
        return [conn for conn in connections]

    def get_connection_count(self) -> int:
        return len(self.get_connections())

    def close_all_connections(self) -> int:
        connections = self.get_connections()
        for connection in connections:
            logging.info(f"gracefully closing {connection['name']}")
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
        queue_details = self.api.get(f"/api/queues/{self.vhost}/{qname}").json()
        return len(queue_details["consumer_details"]) == num_consumers

    def exist_priority_consumer(self, qname, priority) -> bool:
        queue_details = self.api.get(f"/api/queues/{self.vhost}/{qname}").json()
        for consumer in queue_details["consumer_details"]:
            print(
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
        except Exception:
            return None
        if "messages" in queue:
            return queue["messages"]
        elif zero_if_not_exists and queue.get("error") == "Object Not Found":
            return 0
        else:
            return None

    def get_persistent_message_count(self, qname) -> Any:
        try:
            queue = self.api.get(f"/api/queues/{self.vhost}/{qname}/").json()
        except Exception:
            return None
        if "messages_persistent" in queue:
            return queue["messages_persistent"]
        else:
            return None

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
        try:
            self.api.delete(f"/api/queues/{self.vhost}/{qname}")
            self.timed_expect_true(lambda: self.get_message_count(qname) == None)
        except Exception:
            pass

    def delete_vhost(self) -> Any:
        try:
            self.prox.unmapVhost()
            self.api.delete(f"/api/vhosts/{self.vhost}")
        except Exception:
            pass

    def get_exchange_info(self, xname) -> Any:
        try:
            return self.api.get(f"/api/exchanges/{self.vhost}/{xname}/").json()
        except Exception:
            return None

    def get_queue_info(self, qname) -> Any:
        try:
            return self.api.get(f"/api/queues/{self.vhost}/{qname}/").json()
        except Exception:
            return None

    def create_queue(self, qname) -> Any:
        try:
            return self.api.put(
                f"/api/queues/{self.vhost}/{qname}/", json={"durable": True}
            ).json()
        except Exception:
            return None

    def purge_queue(self, qname) -> Any:
        try:
            return self.api.delete(f"/api/queues/{self.vhost}/{qname}/contents").json()
        except Exception:
            return None


class SpoofHungServer:
    """Class to simulate a permanently or temporarily hung port"""

    def __init__(self):
        self._hung_server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # binding to 0 will force the OS to assign an available port
        self._hung_server_socket.bind(("0.0.0.0", 0))
        self._hung_server_socket.listen(1)
        self._dest_socket = None
        self._client_socket = None
        self._terminate = False
        self._forwarding = False
        self._listening_thread = None
        self._forward_thread = None
        self._server_hung = False
        self._spoof_conn = None
        self._spoof_addr = None

    def __del__(self):
        self._terminate = True

    @property
    def spoof_port(self):
        return self._hung_server_socket.getsockname()[1]

    def assert_spoofed(self):
        """assert the spoofed port has accepted a connection"""
        if not self._server_hung:
            raise RuntimeError("Must start_hung_server before testing if spoof worked")
        assert isinstance(self._spoof_conn, socket.socket)
        logging.info("Confirmed connection to hung port.")
        self._server_hung = False

    def hung_serve(self):
        self._server_hung = True
        while self._server_hung:
            self._spoof_conn, self._spoof_addr = self._hung_server_socket.accept()
            logging.info(
                "Connection from {0}:{1} hit hung socket on localhost:{2}.".format(
                    self._spoof_addr[0], self._spoof_addr[1], self.spoof_port
                )
            )

    def start_hung_server(self):
        threading.Thread(target=self.hung_serve).start()

    def start_forwarding(self, dest_host, dest_port):
        """Accepts a connection on the instantiated spoofed port and will transfer packets between
        the connected client and the supplied destination host and port.
        """
        self._server_hung = False

        self._client_socket, addr = self._hung_server_socket.accept()

        self._dest_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._dest_socket.connect((dest_host, dest_port))

        logging.debug(f"forwarding {self._client_socket}->{self._dest_socket}")
        self._listening_thread = threading.Thread(
            target=self.forward, args=[self._client_socket, self._dest_socket]
        )
        self._listening_thread.start()

        logging.debug(f"forwarding {self._client_socket}<-{self._dest_socket}")
        self._forward_thread = threading.Thread(
            target=self.forward, args=[self._dest_socket, self._client_socket]
        )
        self._forward_thread.start()
        self._forwarding = True

    def forward(self, source: socket.socket, dest: socket.socket):
        while not self._terminate:
            data = source.recv(1024)
            if data:
                logging.debug(f"sending data to {source} from {dest}")
                dest.sendall(data)

    def stop(self):
        self._terminate = True
        if self._forwarding:
            while self._listening_thread.is_alive() or self._forward_thread.is_alive():
                logging.info("Waiting for threads to terminate")
                sleep(5)
            self._client_socket.shutdown(socket.SHUT_RDWR)
            self._dest_socket.shutdown(socket.SHUT_RDWR)
            self._hung_server_socket.shutdown(socket.SHUT_RDWR)
