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

#include <rmqamqpt_basicdeliver.h>

#include <rmqamqpt_types.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>
#include <bsl_cstddef.h>

namespace BloombergLP {
namespace rmqamqpt {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.BASICDELIVER")
}

BasicDeliver::BasicDeliver(const bsl::string& consumerTag,
                           uint64_t deliveryTag,
                           bool redelivered,
                           const bsl::string& exchangeName,
                           const bsl::string& routingKey)
: d_consumerTag(consumerTag)
, d_deliveryTag(deliveryTag)
, d_redelivered(redelivered)
, d_exchange(exchangeName)
, d_routingKey(routingKey)
{
}

BasicDeliver::BasicDeliver()
: d_consumerTag()
, d_deliveryTag()
, d_redelivered()
, d_exchange()
, d_routingKey()
{
}

bool BasicDeliver::decode(BasicDeliver* deliver,
                          const uint8_t* data,
                          bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (!Types::decodeShortString(&deliver->d_consumerTag, &buffer)) {
        return false;
    }

    if (sizeof(deliver->d_deliveryTag) + sizeof(deliver->d_redelivered) >
        buffer.available()) {
        BALL_LOG_ERROR
            << "Not enough data to decode BasicDeliver frame: available "
            << buffer.available();
        return false;
    }
    deliver->d_deliveryTag = buffer.copy<bdlb::BigEndianUint64>();
    deliver->d_redelivered = buffer.copy<bool>();

    if (!Types::decodeShortString(&deliver->d_exchange, &buffer)) {
        return false;
    }

    return Types::decodeShortString(&deliver->d_routingKey, &buffer);
}

void BasicDeliver::encode(Writer& output, const BasicDeliver& deliver)
{

    Types::encodeShortString(output, deliver.d_consumerTag);

    Types::write(output, bdlb::BigEndianUint64::make(deliver.d_deliveryTag));
    Types::write(output, deliver.d_redelivered);
    Types::encodeShortString(output, deliver.d_exchange);
    Types::encodeShortString(output, deliver.d_routingKey);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicDeliver& deliver)
{
    return os << "BasicDeliver: [ consumer-tag: " << deliver.consumerTag()
              << ", delivery-tag: " << deliver.deliveryTag()
              << ", redelivered: " << deliver.redelivered()
              << ", exchange: " << deliver.exchange()
              << ", routing-key: " << deliver.routingKey() << "]";
}

} // namespace rmqamqpt
} // namespace BloombergLP
