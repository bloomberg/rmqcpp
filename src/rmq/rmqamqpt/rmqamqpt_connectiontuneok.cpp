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

#include <rmqamqpt_connectiontuneok.h>

#include <rmqamqpt_types.h>

#include <rmqamqpt_buffer.h>

namespace BloombergLP {
namespace rmqamqpt {

ConnectionTuneOk::ConnectionTuneOk()
: d_channelMax(0)
, d_frameMax(0)
, d_heartbeatInterval(0)
{
}

ConnectionTuneOk::ConnectionTuneOk(uint16_t channelMax,
                                   uint32_t frameMax,
                                   uint16_t heartbeatInterval)
: d_channelMax(channelMax)
, d_frameMax(frameMax)
, d_heartbeatInterval(heartbeatInterval)
{
}

bool ConnectionTuneOk::decode(ConnectionTuneOk* tuneOk,
                              const uint8_t* data,
                              bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (sizeof(tuneOk->d_channelMax) + sizeof(tuneOk->d_frameMax) +
            sizeof(tuneOk->d_heartbeatInterval) >
        buffer.available()) {
        return false;
    }

    tuneOk->d_channelMax        = buffer.copy<bdlb::BigEndianUint16>();
    tuneOk->d_frameMax          = buffer.copy<bdlb::BigEndianUint32>();
    tuneOk->d_heartbeatInterval = buffer.copy<bdlb::BigEndianUint16>();
    return true;
}
void ConnectionTuneOk::encode(Writer& output, const ConnectionTuneOk& tuneOk)
{
    Types::write(output, bdlb::BigEndianUint16::make(tuneOk.channelMax()));
    Types::write(output, bdlb::BigEndianUint32::make(tuneOk.frameMax()));
    Types::write(output,
                 bdlb::BigEndianUint16::make(tuneOk.heartbeatInterval()));
}
bsl::ostream& operator<<(bsl::ostream& os, const ConnectionTuneOk& tuneOkMethod)
{
    os << "Connection TuneOk = ["
       << "channel-max: " << tuneOkMethod.channelMax()
       << ", frame-max: " << tuneOkMethod.frameMax()
       << ", heartbeat-interval: " << tuneOkMethod.heartbeatInterval() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
