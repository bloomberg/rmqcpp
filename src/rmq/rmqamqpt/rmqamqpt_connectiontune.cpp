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

#include <rmqamqpt_connectiontune.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>

namespace BloombergLP {
namespace rmqamqpt {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.CONNECTIONTUNE")
}

ConnectionTune::ConnectionTune()
: d_channelMax()
, d_frameMax()
, d_heartbeatInterval()
{
}

ConnectionTune::ConnectionTune(uint16_t channelMax,
                               uint32_t frameMax,
                               uint16_t heartbeatInterval)
: d_channelMax(channelMax)
, d_frameMax(frameMax)
, d_heartbeatInterval(heartbeatInterval)
{
}

bool ConnectionTune::decode(ConnectionTune* tune,
                            const uint8_t* data,
                            bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (sizeof(tune->d_channelMax) + sizeof(tune->d_frameMax) +
            sizeof(tune->d_heartbeatInterval) >
        buffer.available()) {
        BALL_LOG_ERROR
            << "Not enough data to decode ConnectionTune frame: available "
            << buffer.available();
        return false;
    }

    tune->d_channelMax        = buffer.copy<bdlb::BigEndianUint16>();
    tune->d_frameMax          = buffer.copy<bdlb::BigEndianUint32>();
    tune->d_heartbeatInterval = buffer.copy<bdlb::BigEndianUint16>();
    return true;
}
void ConnectionTune::encode(Writer& output, const ConnectionTune& tune)
{
    Types::write(output, bdlb::BigEndianUint16::make(tune.channelMax()));
    Types::write(output, bdlb::BigEndianUint32::make(tune.frameMax()));
    Types::write(output, bdlb::BigEndianUint16::make(tune.heartbeatInterval()));
}

bsl::ostream& operator<<(bsl::ostream& os, const ConnectionTune& tuneMethod)
{
    os << "Connection Tune = ["
       << "channel-max: " << tuneMethod.channelMax()
       << ", frame-max: " << tuneMethod.frameMax()
       << ", heartbeat-interval: " << tuneMethod.heartbeatInterval() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
