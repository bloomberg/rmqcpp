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

#include <rmqt_envelope.h>

#include <bsl_ios.h>

namespace BloombergLP {
namespace rmqt {
Envelope::Envelope(uint64_t deliveryTag,
                   size_t channelLifetimeId,
                   const bsl::string& consumerTag,
                   const bsl::string& exchange,
                   const bsl::string& routingKey,
                   bool redelivered)
: d_deliveryTag(deliveryTag)
, d_channelLifetimeId(channelLifetimeId)
, d_consumerTag(consumerTag)
, d_exchange(exchange)
, d_routingKey(routingKey)
, d_redelivered(redelivered)
{
}
bsl::ostream& operator<<(bsl::ostream& os, const rmqt::Envelope& envelope)
{
    os << "Envelope = [deliveryTag: " << envelope.deliveryTag()
       << ", channelLifetimeId: " << envelope.channelLifetimeId()
       << ", consumerTag: " << envelope.consumerTag()
       << ", exchange: " << envelope.exchange()
       << ", routingKey: " << envelope.routingKey()
       << ", redelivered: " << bsl::boolalpha << envelope.redelivered() << "]";
    return os;
}

} // namespace rmqt
} // namespace BloombergLP
