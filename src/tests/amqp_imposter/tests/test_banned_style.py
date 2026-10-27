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

import ast
from pathlib import Path

SOURCE_ROOT = Path(__file__).parent.parent / "amqp_imposter"


def is_isinstance_invocation(node: ast.expr) -> bool:
    match node:
        case ast.UnaryOp(op=ast.Not(), operand=operand):
            return is_isinstance_invocation(operand)
        case ast.Call(func=ast.Name(id="isinstance")):
            return True
        case _:
            return False


def find_isinstance_branches(source: str) -> list[int]:
    return [
        node.lineno
        for node in ast.walk(ast.parse(source))
        if isinstance(node, ast.If)
        and node.orelse
        and is_isinstance_invocation(node.test)
    ]


def test_ban_isinstance_branches() -> None:
    # Prefer: match statements
    # Rationale: preserve exhaustive typechecking and keep code readable as a finite state machine
    assert not [
        f"{path.relative_to(SOURCE_ROOT.parent)}:{line}"
        for path in sorted(SOURCE_ROOT.rglob("*.py"))
        if "_pb2" not in path.name
        for line in find_isinstance_branches(path.read_text())
    ]
