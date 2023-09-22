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

#include <rmqamqpt_basicnack.h>

#include <bdlb_bigendian.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>

namespace BloombergLP {
namespace rmqamqpt {

namespace {
const uint8_t BITMASK_BIT_0 = (1 << 0);
const uint8_t BITMASK_BIT_1 = (1 << 1);
} // namespace

BasicNack::BasicNack(bsl::uint64_t dt, bool requeue, bool multiple)
: d_deliveryTag(dt)
, d_requeue(requeue)
, d_multiple(multiple)
{
}

BasicNack::BasicNack()
: d_deliveryTag()
, d_requeue()
, d_multiple()
{
}

bool BasicNack::decode(BasicNack* nack,
                       const uint8_t* data,
                       bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (buffer.available() < sizeof(bdlb::BigEndianUint64)) {
        return false;
    }

    nack->d_deliveryTag = buffer.copy<bdlb::BigEndianUint64>();

    if (buffer.available() < sizeof(uint8_t)) {
        return false;
    }

    uint8_t bitmask  = buffer.copy<uint8_t>();
    nack->d_multiple = bitmask & BITMASK_BIT_0;
    nack->d_requeue  = bitmask & BITMASK_BIT_1;

    return true;
}

void BasicNack::encode(Writer& output, const BasicNack& nack)
{
    Types::write(output, bdlb::BigEndianUint64::make(nack.d_deliveryTag));

    uint8_t bitmask = 0;
    if (nack.d_multiple) {
        bitmask |= BITMASK_BIT_0;
    }
    if (nack.d_requeue) {
        bitmask |= BITMASK_BIT_1;
    }

    Types::write(output, bitmask);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicNack& nack)
{
    return os << "BasicNack:"
              << "[ delivery-tag: " << nack.deliveryTag()
              << ", multiple: " << nack.multiple()
              << ", requeue: " << nack.requeue() << "]";
}

} // namespace rmqamqpt
} // namespace BloombergLP
