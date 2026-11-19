// Copyright 2020-2023 Bloomberg Finance L.P.
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <rmqt_consumerack.h>

#include <rmqt_envelope.h>

#include <bslmf_assert.h>

namespace BloombergLP {
namespace rmqt {

// Until REJECT and REQUEUE can be retired
BSLMF_ASSERT(ConsumerAck::REJECT == ConsumerAck::NACK_REQUEUE_FALSE);
BSLMF_ASSERT(ConsumerAck::REQUEUE == ConsumerAck::NACK_REQUEUE_TRUE);

ConsumerAck::ConsumerAck(const Envelope& envelope, Type type)
: d_envelope(envelope)
, d_type(type)
{
}

ConsumerAck::~ConsumerAck() {}

bool ConsumerAck::usesBasicReject(Type type)
{
    return type == REJECT_REQUEUE_FALSE || type == REJECT_REQUEUE_TRUE;
}

bool ConsumerAck::supportsMultiple(Type type) { return !usesBasicReject(type); }

bsl::ostream& operator<<(bsl::ostream& os, ConsumerAck::Type type)
{
    switch (type) {
        case ConsumerAck::ACK:
            return os << "ACK";
        case ConsumerAck::NACK_REQUEUE_FALSE:
            return os << "NACK_REQUEUE_FALSE";
        case ConsumerAck::NACK_REQUEUE_TRUE:
            return os << "NACK_REQUEUE_TRUE";
        case ConsumerAck::REJECT_REQUEUE_FALSE:
            return os << "REJECT_REQUEUE_FALSE";
        case ConsumerAck::REJECT_REQUEUE_TRUE:
            return os << "REJECT_REQUEUE_TRUE";
    }
    return os << "UNKNOWN(" << static_cast<int>(type) << ")";
}

} // namespace rmqt
} // namespace BloombergLP
