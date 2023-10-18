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

#include <rmqamqpt_basicqos.h>

#include <rmqamqpt_types.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>
#include <bsl_cstddef.h>

namespace BloombergLP {
namespace rmqamqpt {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.BASICQOS")
}

BasicQoS::BasicQoS(bsl::uint16_t prefetchCount,
                   bsl::uint32_t prefetchSize,
                   bool global)
: d_prefetchSize(prefetchSize)
, d_prefetchCount(prefetchCount)
, d_global(global)
{
}

BasicQoS::BasicQoS()
: d_prefetchSize()
, d_prefetchCount()
, d_global()
{
}

bool BasicQoS::decode(BasicQoS* qos,
                      const uint8_t* data,
                      bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (sizeof(qos->d_prefetchSize) + sizeof(qos->d_prefetchCount) +
            sizeof(qos->d_global) >
        buffer.available()) {
        BALL_LOG_ERROR
            << "Not enough data to decode ConnectionTune frame: available "
            << buffer.available();
        return false;
    }

    qos->d_prefetchSize  = buffer.copy<bdlb::BigEndianUint32>();
    qos->d_prefetchCount = buffer.copy<bdlb::BigEndianUint16>();
    qos->d_global        = buffer.copy<bool>();
    return true;
}

void BasicQoS::encode(Writer& output, const BasicQoS& qos)
{
    Types::write(output, bdlb::BigEndianUint32::make(qos.prefetchSize()));
    Types::write(output, bdlb::BigEndianUint16::make(qos.prefetchCount()));
    Types::write(output, qos.global());
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicQoS& qos)
{
    return os << "BasicQos: [ prefetch-size: " << qos.prefetchSize()
              << ", prefetch-count: " << qos.prefetchCount()
              << ", global: " << qos.global() << "]";
}

} // namespace rmqamqpt
} // namespace BloombergLP
