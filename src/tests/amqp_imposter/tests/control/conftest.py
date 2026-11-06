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

import shutil
import tempfile
from collections.abc import Iterator
from pathlib import Path

import pytest

from .helpers import SHORT_TEMP_DIR


@pytest.fixture
def socket_path() -> Iterator[Path]:
    base_path = tempfile.mkdtemp(dir=SHORT_TEMP_DIR)
    yield Path(base_path) / "control.sock"
    shutil.rmtree(base_path, ignore_errors=True)
