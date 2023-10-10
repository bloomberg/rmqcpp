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

#include <rmqamqpt_basicpublish.h>
#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bdlb_bigendian.h>
#include <bsl_cstddef.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqamqpt {

namespace {
const uint8_t MASK_BIT_1 = 1 << 1;
const uint8_t MASK_BIT_0 = 1 << 0;
} // namespace

BasicPublish::BasicPublish(const bsl::string& exchangeName,
                           const bsl::string& routingKey,
                           bool mandatory,
                           bool immediate)
: d_exchangeName(exchangeName)
, d_routingKey(routingKey)
, d_mandatory(mandatory)
, d_immediate(immediate)
{
}

BasicPublish::BasicPublish()
: d_exchangeName()
, d_routingKey()
, d_mandatory()
, d_immediate()
{
}

bool BasicPublish::decode(BasicPublish* publish,
                          const uint8_t* data,
                          bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);
    if (sizeof(int16_t) > buffer.available()) {
        return false;
    }
    buffer.copy<bdlb::BigEndianInt16>();
    if (!Types::decodeShortString(&publish->d_exchangeName, &buffer)) {
        return false;
    }
    if (!Types::decodeShortString(&publish->d_routingKey, &buffer)) {
        return false;
    }
    if (sizeof(uint8_t) > buffer.available()) {
        return false;
    }

    const uint8_t theByte = buffer.copy<uint8_t>();

    publish->d_mandatory = theByte & MASK_BIT_0;
    publish->d_immediate = theByte & MASK_BIT_1;

    return true;
}

void BasicPublish::encode(Writer& output, const BasicPublish& publish)
{
    Types::write(output, bdlb::BigEndianInt16::make(0));
    Types::encodeShortString(output, publish.d_exchangeName);
    Types::encodeShortString(output, publish.d_routingKey);

    uint8_t theByte = 0;

    if (publish.d_mandatory) {
        theByte |= MASK_BIT_0;
    }
    if (publish.d_immediate) {
        theByte |= MASK_BIT_1;
    }

    Types::write(output, theByte);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicPublish& publish)
{
    return os << "BasicPublish: [ "
              << ", exchange-name: " << publish.exchangeName()
              << ", routing-key: " << publish.routingKey()
              << ", mandatory: " << publish.mandatory()
              << ", immediate: " << publish.immediate() << "]";
}

bool operator==(const BasicPublish& lhs, const BasicPublish& rhs)
{
    return (&lhs == &rhs) || (lhs.exchangeName() == rhs.exchangeName() &&
                              lhs.routingKey() == rhs.routingKey() &&
                              lhs.mandatory() == rhs.mandatory() &&
                              lhs.immediate() == rhs.immediate());
}

} // namespace rmqamqpt
} // namespace BloombergLP
