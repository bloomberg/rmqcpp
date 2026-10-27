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

import os
import subprocess
from collections.abc import Generator, Mapping, Sequence
from contextlib import contextmanager
from dataclasses import dataclass
from itertools import chain

from amqp_imposter.control.client import AmqpImposter, Transcript, pb

TIMEOUT_CLIENT = 300.0

CLIENT_VHOST = "fuzz"

# TODO amqp_imposter adhere to negotiated parameters
NEGOTIATION_OPTIONS = pb.NegotiationOptions(heartbeat=60)


@dataclass(frozen=True, slots=True)
class ImposterCapture:
    transcript: Transcript
    rc: int | None


def perftest_args(options: Mapping[str, str]) -> tuple[str, ...]:
    return tuple(chain.from_iterable(options.items()))


@contextmanager
def kill_on_exit[T: (str, bytes)](
    process: subprocess.Popen[T],
) -> Generator[subprocess.Popen[T]]:
    with process:
        try:
            yield process
        finally:
            process.kill()


def run_client(
    imposter: AmqpImposter,
    args: Sequence[str],
    timeout: float = TIMEOUT_CLIENT,
) -> ImposterCapture:
    uri = f"amqp://guest:guest@127.0.0.1:{imposter.amqp_port}/{CLIENT_VHOST}"
    cmd = [os.environ["RMQ_EXE"], *args, "--uri", uri]
    env = {**os.environ, "PERF_TEST_LOG": "TRACE"}

    with kill_on_exit(subprocess.Popen(cmd, env=env)) as client:
        try:
            rc = client.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            rc = None
        return ImposterCapture(imposter.transcript(), rc)
