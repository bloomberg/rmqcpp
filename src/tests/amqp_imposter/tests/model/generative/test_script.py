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

import pytest

from amqp_imposter.model import (
    GenState,
    Received,
    ScriptRule,
    Sent,
    WeightedPattern,
)
from amqp_imposter.model import method_weights as mw
from amqp_imposter.model import methods as m

from ..helpers import confirming, consume, consuming, publish

SAMPLES = 100
MAX_SENDS = 5
STOP_WEIGHT_LONG_BURSTS = 1

WEIGHTS_NACK_NEVER = (WeightedPattern(m.BasicNack(), 0),)
WEIGHTS_ALL_NEVER = tuple(WeightedPattern(kind(), 0) for kind in m.METHOD_KINDS)

SCRIPT_NACK = (ScriptRule(Received(m.BasicPublish()), then_send=m.BasicNack()),)
SCRIPT_DELIVERY_LOOP = (
    ScriptRule(Received(m.BasicPublish()), then_send=m.BasicDeliver()),
    ScriptRule(Sent(m.BasicDeliver()), then_send=m.BasicDeliver()),
)


def test_received_overrides_weights() -> None:
    for seed in range(SAMPLES):
        server = confirming()
        server.gen_state = GenState(seed)
        publish(server)

        burst = server.send_gens(SCRIPT_NACK, WEIGHTS_NACK_NEVER)

        assert isinstance(burst[0], m.BasicNack)


def test_received_not_after_send() -> None:
    for seed in range(SAMPLES):
        server = confirming()
        server.gen_state = GenState(seed)
        publish(server)
        publish(server)
        server.send(m.BasicAck(delivery_tag=2))

        burst = server.send_gens(SCRIPT_NACK, WEIGHTS_NACK_NEVER)

        assert not any(isinstance(sent, m.BasicNack) for sent in burst)


def test_received_no_match() -> None:
    server = confirming()
    publish(server)
    script = (ScriptRule(Received(m.BasicGet()), then_send=m.BasicNack()),)

    burst = server.send_gens(script, WEIGHTS_NACK_NEVER)

    assert not any(isinstance(sent, m.BasicNack) for sent in burst)


def test_sent_does_not_match_received() -> None:
    server = confirming()
    publish(server)
    script = (ScriptRule(Sent(m.BasicPublish()), then_send=m.BasicNack()),)

    burst = server.send_gens(script, WEIGHTS_NACK_NEVER)

    assert not any(isinstance(sent, m.BasicNack) for sent in burst)


@pytest.mark.parametrize(("delivery_tag", "fires"), ((1, False), (2, True)))
def test_when_exact_fields(delivery_tag: int, fires: bool) -> None:
    server = confirming()
    publish(server)
    publish(server)
    server.send(m.BasicAck(delivery_tag=delivery_tag))
    script = (
        ScriptRule(
            Sent(m.BasicAck(delivery_tag=2)), then_send=mw.UNPROMPTED_CONNECTION_CLOSE
        ),
    )

    sent = server.send_gen(script)

    assert isinstance(sent, m.ConnectionClose) == fires


def test_received_not_sendable() -> None:
    server = confirming()
    publish(server)
    script = (
        ScriptRule(Received(m.BasicPublish()), then_send=m.BasicDeliver()),
        ScriptRule(Received(m.BasicPublish()), then_send=m.BasicAck()),
        ScriptRule(Received(m.BasicPublish()), then_send=m.BasicNack()),
    )

    burst = server.send_gens(script=script)

    assert isinstance(burst[0], m.BasicAck)


def test_received_not_sendable_fallback() -> None:
    server = confirming()
    publish(server)
    script = (ScriptRule(Received(m.BasicPublish()), then_send=m.BasicDeliver()),)

    burst = server.send_gens(script=script)

    assert not any(isinstance(sent, m.BasicDeliver) for sent in burst)


def test_then_send_updating_state() -> None:
    acks: set[m.BasicAck] = set()

    for seed in range(SAMPLES):
        server = confirming()
        server.gen_state = GenState(seed)
        publish(server)
        publish(server)
        publish(server)
        script = (ScriptRule(Received(m.BasicPublish()), then_send=m.BasicAck()),)

        sent = server.send_gen(script)

        assert isinstance(sent, m.BasicAck)
        acks.add(sent)

    assert len(acks) > 1


def test_sent_chain() -> None:
    server = confirming()
    consume(server)
    publish(server)
    script = (
        ScriptRule(Received(m.BasicPublish()), then_send=m.BasicAck()),
        ScriptRule(Sent(m.BasicAck()), then_send=m.BasicDeliver()),
    )

    burst = server.send_gens(script=script)

    assert [type(sent) for sent in burst[:2]] == [m.BasicAck, m.BasicDeliver]


def test_not_across_bursts() -> None:
    server = confirming()
    publish(server)

    assert server.send_gens(weighted_patterns=WEIGHTS_ALL_NEVER) == []
    # should've passed in the rule before!
    assert server.send_gens(SCRIPT_NACK, WEIGHTS_ALL_NEVER) == []


def test_max_sends() -> None:
    server = consuming(no_ack=True)
    publish(server)

    burst = server.send_gens(script=SCRIPT_DELIVERY_LOOP, max_sends=MAX_SENDS)

    assert len(burst) == MAX_SENDS
    assert all(isinstance(sent, m.BasicDeliver) for sent in burst)

    # once broken, the cause is forgotten
    assert server.send_gens(SCRIPT_DELIVERY_LOOP, WEIGHTS_ALL_NEVER) == []


def test_max_sends_sampled() -> None:
    for seed in range(SAMPLES):
        server = consuming(no_ack=True)
        server.gen_state = GenState(seed)

        burst = server.send_gens(
            stop_weight=STOP_WEIGHT_LONG_BURSTS, max_sends=MAX_SENDS
        )

        assert len(burst) <= MAX_SENDS


def test_arbitrary_not_matchable() -> None:
    with pytest.raises(m.BadPattern):
        ScriptRule(Received(m.BasicPublish(mandatory=True)), then_send=m.BasicAck())

    with pytest.raises(m.BadPattern):
        ScriptRule(Sent(m.BasicAck()), then_send=m.BasicDeliver(redelivered=True))
