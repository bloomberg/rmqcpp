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

from . import generators as g
from . import method_bag as mb
from . import method_sampling as msa
from . import method_scripts as scr
from . import method_unification as mu
from . import method_weights as mw
from . import methods as m
from . import model_state as ms
from . import negotiation_options as no
from . import transition_receive, transition_send


class ModelServer:
    """
    Models a one-channel-at-a-time AMQP 0-9-1 server state machine.
    """

    def __init__(
        self,
        negotiation_options: no.NegotiationOptions = no.DEFAULT_NEGOTIATION_OPTIONS,
        gen_state: g.GenState = g.DEFAULT_GEN_STATE,
    ) -> None:
        self.model_state = ms.ModelState(negotiation_options=negotiation_options)
        self.gen_state = gen_state

        self._remember_when: scr.Received | scr.Sent | None = None

    def receive_protocol_header(self) -> ms.Unrepresentable | None:
        match transition_receive.apply_protocol_header(self.model_state):
            case ms.Unrepresentable() as unrepr:
                return unrepr

            case ms.ModelState() as model_state:
                self.model_state = model_state
                return None

    def receive(self, method: m.Method) -> m.Method | ms.Unrepresentable:
        if m.is_method_pattern(method):
            return ms.Unrepresentable(method)

        receivable, gen_state = mu.concretise(method, self.gen_state)

        match transition_receive.apply(self.model_state, receivable):
            case ms.Unrepresentable() as unrepr:
                return unrepr

            case ms.ModelState() as model_state:
                self.model_state = model_state
                self.gen_state = gen_state.advance()
                self._remember_when = scr.Received(receivable)
                return receivable

    def send(self, method: m.Method) -> m.Method | ms.Unrepresentable:
        sendable = mu.unify_any(method, self.sendables())

        if sendable is None:
            return ms.Unrepresentable(method)

        sendable, gen_state = mu.concretise(sendable, self.gen_state)

        match transition_send.apply(self.model_state, sendable):
            case ms.Unrepresentable() as unrepr:
                return unrepr

            case ms.ModelState() as model_state:
                self.model_state = model_state
                self.gen_state = gen_state.advance()
                self._remember_when = scr.Sent(sendable)
                return sendable

    def send_gen(
        self,
        script: tuple[scr.ScriptRule, ...] = (),
        weighted_patterns: tuple[mw.WeightedPattern, ...] = mw.DEFAULT_WEIGHTS,
        stop_weight: int = mw.STOP_WEIGHT,
    ) -> m.Method | msa.Stop:
        rules = (
            scr.match_rules(script, self._remember_when)
            if self._remember_when is not None
            else ()
        )

        for rule in rules:
            thens = [
                unified
                for sendable in self.sendables()
                if (unified := mu.unify(rule.then_send, sendable)) is not None
            ]
            match thens:
                case [t, *ts]:
                    chosen, self.gen_state = g.Choice((t, *ts)).generate(self.gen_state)
                    concrete = self.send(chosen)

                    assert not isinstance(concrete, ms.Unrepresentable), concrete
                    return concrete

        sampler = msa.make_sampler(self.sendables(), weighted_patterns, stop_weight)
        sampled, self.gen_state = sampler.generate(self.gen_state)

        match sampled:
            case msa.Stop():
                return sampled

            case method:
                concrete = self.send(method)
                assert not isinstance(concrete, ms.Unrepresentable), concrete
                return concrete

    def send_gens(
        self,
        script: tuple[scr.ScriptRule, ...] = (),
        weighted_patterns: tuple[mw.WeightedPattern, ...] = mw.DEFAULT_WEIGHTS,
        stop_weight: int = mw.STOP_WEIGHT,
        max_sends: int = msa.DEFAULT_MAX_SENDS,
    ) -> list[m.Method]:
        methods: list[m.Method] = []

        while len(methods) < max_sends:
            match self.send_gen(script, weighted_patterns, stop_weight):
                case msa.Stop():
                    break

                case method:
                    methods.append(method)

        self._remember_when = None
        return methods

    def sendables(self) -> tuple[m.Method, ...]:
        return mb.sendables(self.model_state)
