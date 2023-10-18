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

#include <rmqamqpt_basicack.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bdlb_bigendian.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>

namespace BloombergLP {
namespace rmqamqpt {

BasicAck::BasicAck(bsl::uint64_t dt, bool multiple)
: d_deliveryTag(dt)
, d_multiple(multiple)
{
}

BasicAck::BasicAck()
: d_deliveryTag()
, d_multiple()
{
}

bool BasicAck::decode(BasicAck* ack,
                      const uint8_t* data,
                      bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (buffer.available() < sizeof(bdlb::BigEndianUint64)) {

        return false;
    }

    ack->d_deliveryTag = buffer.copy<bdlb::BigEndianUint64>();

    if (buffer.available() < sizeof(bool)) {
        return false;
    }

    ack->d_multiple = buffer.copy<bool>();

    return true;
}

void BasicAck::encode(Writer& output, const BasicAck& ack)
{
    Types::write(output, bdlb::BigEndianUint64::make(ack.d_deliveryTag));
    Types::write(output, ack.d_multiple);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicAck& ack)
{
    return os << "BasicAck:"
              << "[ delivery-tag: " << ack.deliveryTag()
              << ", multiple: " << ack.multiple() << "]";
}

} // namespace rmqamqpt
} // namespace BloombergLP
