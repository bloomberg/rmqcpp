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

from ..v1 import imposter_pb2 as pb

type PatternMethod = (
    pb.ConnectionStartPattern
    | pb.ConnectionStartOkPattern
    | pb.ConnectionTunePattern
    | pb.ConnectionTuneOkPattern
    | pb.ConnectionOpenPattern
    | pb.ConnectionOpenOkPattern
    | pb.ConnectionClosePattern
    | pb.ConnectionCloseOkPattern
    | pb.ChannelOpenPattern
    | pb.ChannelOpenOkPattern
    | pb.ChannelClosePattern
    | pb.ChannelCloseOkPattern
    | pb.ConfirmSelectPattern
    | pb.ConfirmSelectOkPattern
    | pb.ExchangeDeclarePattern
    | pb.ExchangeDeclareOkPattern
    | pb.ExchangeBindPattern
    | pb.ExchangeBindOkPattern
    | pb.QueueDeclarePattern
    | pb.QueueDeclareOkPattern
    | pb.QueueBindPattern
    | pb.QueueBindOkPattern
    | pb.BasicQosPattern
    | pb.BasicQosOkPattern
    | pb.BasicConsumePattern
    | pb.BasicConsumeOkPattern
    | pb.BasicCancelPattern
    | pb.BasicCancelOkPattern
    | pb.BasicPublishPattern
    | pb.BasicReturnPattern
    | pb.BasicAckPattern
    | pb.BasicNackPattern
    | pb.BasicRejectPattern
    | pb.BasicDeliverPattern
    | pb.BasicGetPattern
    | pb.BasicGetOkPattern
    | pb.BasicGetEmptyPattern
)

PATTERN_FIELD_NAMES: frozendict[type[PatternMethod], str] = frozendict(
    (type(getattr(pb.Pattern(), field.name)), field.name)
    for field in pb.Pattern.DESCRIPTOR.oneofs_by_name["method"].fields
)


def pattern(method: PatternMethod) -> pb.Pattern:
    wrapped = pb.Pattern()
    getattr(wrapped, PATTERN_FIELD_NAMES[type(method)]).CopyFrom(method)
    return wrapped


def weighted(method: PatternMethod, weight: int | None = None) -> pb.Weighted:
    return pb.Weighted(pattern=pattern(method), weight=weight)


def when_receive(method: PatternMethod, then_send: PatternMethod) -> pb.Scripted:
    return pb.Scripted(received=pattern(method), then_send=pattern(then_send))


def when_send(method: PatternMethod, then_send: PatternMethod) -> pb.Scripted:
    return pb.Scripted(sent=pattern(method), then_send=pattern(then_send))
