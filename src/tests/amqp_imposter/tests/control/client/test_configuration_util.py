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

from amqp_imposter.control import client as cl
from amqp_imposter.control.v1 import imposter_pb2 as pb


def test_wrap_selector_method() -> None:
    for field in pb.Selector.DESCRIPTOR.oneofs_by_name["method"].fields:
        method = getattr(pb.Selector(), field.name)

        assert cl.selector(method) == pb.Selector(**{field.name: method})


def test_wrap_pattern_method() -> None:
    for field in pb.Pattern.DESCRIPTOR.oneofs_by_name["method"].fields:
        method = getattr(pb.Pattern(), field.name)

        assert cl.pattern(method) == pb.Pattern(**{field.name: method})
