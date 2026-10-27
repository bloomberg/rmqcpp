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

from dataclasses import fields, replace

from amqp_imposter.model import generators as g
from amqp_imposter.model import method_unification as mu
from amqp_imposter.model import methods as m

from ..helpers import FRESH_DELIVERY


def test_no_arbitraries_remain() -> None:
    concrete, _ = mu.concretise(FRESH_DELIVERY, g.DEFAULT_GEN_STATE)

    assert all(
        not isinstance(getattr(concrete, field.name), g.Gen)
        for field in fields(concrete)
    )


def test_nothing_to_do() -> None:
    concrete, _ = mu.concretise(m.ChannelOpenOk(), g.DEFAULT_GEN_STATE)

    assert concrete == m.ChannelOpenOk()


def test_deterministic() -> None:
    assert mu.concretise(FRESH_DELIVERY, g.DEFAULT_GEN_STATE) == mu.concretise(
        FRESH_DELIVERY, g.DEFAULT_GEN_STATE
    )


def test_predefined_fields_remain() -> None:
    concrete, _ = mu.concretise(FRESH_DELIVERY, g.DEFAULT_GEN_STATE)

    assert concrete == replace(
        FRESH_DELIVERY,
        redelivered=concrete.redelivered,
        exchange=concrete.exchange,
        routing_key=concrete.routing_key,
        properties=concrete.properties,
        body=concrete.body,
    )


def test_concretise_but_one_field() -> None:
    free, free_gen_state = mu.concretise(FRESH_DELIVERY, g.DEFAULT_GEN_STATE)

    # now one where an arbitrary field has already become concrete
    concrete, concrete_gen_state = mu.concretise(
        replace(FRESH_DELIVERY, exchange="ex"), g.DEFAULT_GEN_STATE
    )

    assert concrete.exchange == "ex"
    assert (concrete.routing_key, concrete.properties, concrete.body) == (
        free.routing_key,
        free.properties,
        free.body,
    )
    assert concrete_gen_state == free_gen_state


def test_advance() -> None:
    _, gen_state = mu.concretise(FRESH_DELIVERY, g.DEFAULT_GEN_STATE)

    assert gen_state != g.DEFAULT_GEN_STATE
