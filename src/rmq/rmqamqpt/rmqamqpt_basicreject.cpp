// Copyright 2026 Bloomberg Finance L.P.
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

#include <rmqamqpt_basicreject.h>

#include <bdlb_bigendian.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>

namespace BloombergLP {
namespace rmqamqpt {

namespace {
/// Basic.Reject's bitmask holds a single bit, `requeue`, at bit 0
const uint8_t BITMASK_BIT_0 = (1 << 0);
} // namespace

BasicReject::BasicReject(bsl::uint64_t dt, bool requeue)
: d_deliveryTag(dt)
, d_requeue(requeue)
{
}

BasicReject::BasicReject()
: d_deliveryTag()
, d_requeue()
{
}

bool BasicReject::decode(BasicReject* reject,
                         const uint8_t* data,
                         bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    uint8_t bitmask = 0;
    if (!buffer.copy<bdlb::BigEndianUint64>(&reject->d_deliveryTag) ||
        !buffer.copy<uint8_t>(&bitmask)) {
        return false;
    }

    reject->d_requeue = bitmask & BITMASK_BIT_0;

    return true;
}

void BasicReject::encode(Writer& output, const BasicReject& reject)
{
    Types::write(output, bdlb::BigEndianUint64::make(reject.d_deliveryTag));

    uint8_t bitmask = 0;
    if (reject.d_requeue) {
        bitmask |= BITMASK_BIT_0;
    }

    Types::write(output, bitmask);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicReject& reject)
{
    return os << "BasicReject:"
              << "[ delivery-tag: " << reject.deliveryTag()
              << ", requeue: " << reject.requeue() << "]";
}

} // namespace rmqamqpt
} // namespace BloombergLP
