# Copyright 2026 Bloomberg Finance L.P.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from pathlib import Path

import pytest

from amqp_imposter.control import transport as tp

ENDPOINTS = tp.ImposterEndpoints(Path("/tmp/amqp-imposter-x/control.sock"), 5672)


def test_endpoints_compose_from_to() -> None:
    assert tp.endpoints_from_json_line(ENDPOINTS.to_json_line()) == ENDPOINTS


@pytest.mark.parametrize(
    "line",
    ("", "not json", '{"socket": "/s"}', '{"socket": 1, "amqp_port": 2}'),
    ids=("empty", "not_json", "missing_port", "wrong_types"),
)
def test_bad_endpoints_line(line: str) -> None:
    assert tp.endpoints_from_json_line(line) is None
