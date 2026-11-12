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
    ModelServer,
    Unrepresentable,
    WeightedPattern,
)
from amqp_imposter.model import method_fields as mf
from amqp_imposter.model import method_sampling as msa
from amqp_imposter.model import method_weights as mw
from amqp_imposter.model import methods as m

from ..helpers import (
    CONSUMER_TAG,
    EXCHANGE,
    GREETED_STATES,
    QUEUE,
    MakeServer,
    channelled,
    confirming,
    connected,
    consuming,
    polling,
    publish,
    receive_unwrap,
    subsets_any_sendable,
    tuned,
)

SAMPLES = 100
WEIGHT = 16
PREFETCH_COUNT = 1000  # so that we don't run out
STOP_WEIGHT_LONG_BURSTS = 1

OPTIONAL_ERROR_CAUSES: tuple[tuple[MakeServer, m.Method], ...] = (
    (tuned, m.ConnectionOpen()),
    (connected, m.ChannelOpen()),
    (channelled, m.ExchangeDeclare(exchange=EXCHANGE, passive=True)),
    (channelled, m.ExchangeDeclare(exchange=EXCHANGE, no_wait=True)),
    (channelled, m.ExchangeBind(destination=EXCHANGE, source=EXCHANGE, no_wait=True)),
    (channelled, m.QueueDeclare(queue=QUEUE, passive=True)),
    (channelled, m.QueueDeclare(queue=QUEUE, no_wait=True)),
    (channelled, m.QueueBind(queue=QUEUE, exchange=EXCHANGE, no_wait=True)),
    (channelled, m.BasicConsume(queue=QUEUE, consumer_tag=CONSUMER_TAG, no_wait=True)),
    (channelled, m.BasicGet(queue=QUEUE)),
    (channelled, m.BasicPublish(exchange=EXCHANGE)),
)
FORCED_ERROR_CAUSES: tuple[tuple[MakeServer, m.Method], ...] = (
    (consuming, m.BasicAck(delivery_tag=0, multiple=False)),
    (channelled, m.ChannelOpen()),
    (connected, m.ConfirmSelect()),
    (channelled, m.BasicQos(prefetch_size=1)),
    (channelled, m.BasicPublish(exchange=EXCHANGE, immediate=True)),
)


def param_name(value: object) -> str:
    return getattr(value, "__name__", type(value).__name__)


def polling_seeded(seed: int) -> ModelServer:
    server = polling()
    server.gen_state = GenState(seed)
    return server


def consuming_seeded(seed: int) -> ModelServer:
    server = consuming(prefetch_count=PREFETCH_COUNT)
    server.gen_state = GenState(seed)
    return server


def connected_seeded(seed: int) -> ModelServer:
    server = connected()
    server.gen_state = GenState(seed)
    return server


def test_before_header() -> None:
    assert isinstance(ModelServer().send_gen(), msa.Stop)
    assert ModelServer().send_gens() == []


def test_no_default_unprompted_teardown() -> None:
    server = consuming()

    assert subsets_any_sendable(
        server,
        m.ConnectionClose,
        class_id=mf.UNPROMPTED_CLASS_ID,
        method_id=mf.UNPROMPTED_METHOD_ID,
    )
    assert subsets_any_sendable(server, m.BasicCancel)

    for _ in range(SAMPLES):
        assert not isinstance(
            server.send_gen(), (m.ConnectionClose, m.ChannelClose, m.BasicCancel)
        )


def test_no_forced_teardown() -> None:
    teardown = (WeightedPattern(mw.UNPROMPTED_CONNECTION_CLOSE, WEIGHT),)

    sent = {
        type(connected_seeded(seed).send_gen(weighted_patterns=teardown))
        for seed in range(SAMPLES)
    }

    assert sent == {m.ConnectionClose, msa.Stop}


@pytest.mark.parametrize(("make_server", "cause"), FORCED_ERROR_CAUSES, ids=param_name)
def test_owed_close_not_too_late(make_server: MakeServer, cause: m.Method) -> None:
    server = make_server()

    receive_unwrap(server, cause)

    sampler = [server.send_gen() for _ in range(SAMPLES)]

    assert any(
        isinstance(sent, (m.ChannelClose, m.ConnectionClose)) for sent in sampler
    )


@pytest.mark.parametrize(
    ("make_server", "cause"), OPTIONAL_ERROR_CAUSES, ids=param_name
)
def test_no_default_optional_errors(make_server: MakeServer, cause: m.Method) -> None:
    for seed in range(SAMPLES):
        server = make_server()
        server.gen_state = GenState(seed)
        receive_unwrap(server, cause)

        assert not isinstance(server.send_gen(), (m.ChannelClose, m.ConnectionClose))


def test_enable_optional_error() -> None:
    not_found = (
        WeightedPattern(m.ChannelClose(reply_code=mf.ReplyCode.NOT_FOUND), WEIGHT),
    )

    sent = {
        type(polling_seeded(seed).send_gen(weighted_patterns=not_found))
        for seed in range(SAMPLES)
    }

    assert m.ChannelClose in sent


def test_pattern_too_specific() -> None:
    server = consuming()
    specific = WeightedPattern(m.BasicCancel(consumer_tag=CONSUMER_TAG), WEIGHT)

    sampler = msa.make_sampler(server.sendables(), (specific,), mw.STOP_WEIGHT)

    cancels = [
        (wp.pattern, len(unified))
        for wp, unified in sampler.unified_by_weighted_pattern
        if isinstance(wp.pattern, m.BasicCancel)
    ]
    assert cancels == [(specific.pattern, 1)]


def test_enable_unprompted_cancel() -> None:
    server = consuming()
    unprompted_cancel = (WeightedPattern(m.BasicCancel(), WEIGHT),)

    sampler = [
        server.send_gen(weighted_patterns=unprompted_cancel) for _ in range(SAMPLES)
    ]

    assert any(isinstance(sent, m.BasicCancel) for sent in sampler)


def test_spurious_weighted_pattern() -> None:
    server = consuming()

    spurious = (WeightedPattern(m.BasicCancel(consumer_tag="other"), WEIGHT),)

    sampler = msa.make_sampler(server.sendables(), spurious, mw.STOP_WEIGHT)

    weights = [wp.weight for wp, _ in sampler.unified_by_weighted_pattern]
    assert WEIGHT not in weights


def test_deterministic() -> None:
    def take_samples(seed: int) -> list[str]:
        server = consuming_seeded(seed)
        return [repr(server.send_gen()) for _ in range(SAMPLES)]

    SEED = 1
    OTHER_SEED = 2

    assert take_samples(SEED) == take_samples(SEED)
    assert take_samples(SEED) != take_samples(OTHER_SEED)


@pytest.mark.parametrize("make_server", GREETED_STATES, ids=lambda b: b.__name__)
def test_never_sample_unrepresentable(make_server: MakeServer) -> None:
    server = make_server()

    for _ in range(SAMPLES):
        assert not isinstance(server.send_gen(), Unrepresentable)


def test_never_sample_zero() -> None:

    def first_sample(seed: int) -> m.Method | msa.Stop:
        server = confirming()
        server.gen_state = GenState(seed)

        publish(server)
        publish(server)
        publish(server)
        publish(server)

        never_sample_tag_2 = (
            WeightedPattern(m.BasicAck(delivery_tag=2), 0),
            WeightedPattern(m.BasicNack(delivery_tag=2), 0),
        )
        return server.send_gen(weighted_patterns=never_sample_tag_2)

    confirmed = {
        sent.delivery_tag
        for seed in range(SAMPLES)
        if isinstance(sent := first_sample(seed), (m.BasicAck, m.BasicNack))
    }

    assert 2 not in confirmed


def test_gens_deterministic() -> None:
    def send_long_burst(seed: int) -> list[m.Method]:
        return consuming_seeded(seed).send_gens(stop_weight=STOP_WEIGHT_LONG_BURSTS)

    assert send_long_burst(1) == send_long_burst(1)
    assert send_long_burst(1) != send_long_burst(2)


@pytest.mark.parametrize("stop_weight", (0, -1))
def test_bad_stop_weight(stop_weight: int) -> None:
    with pytest.raises(mw.BadWeight):
        consuming().send_gen(stop_weight=stop_weight)
