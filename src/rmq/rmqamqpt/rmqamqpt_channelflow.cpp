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

#include <rmqamqpt_channelflow.h>

#include <rmqamqpt_types.h>

#include <ball_log.h>
#include <bsl_cstddef.h>

namespace BloombergLP {
namespace rmqamqpt {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.CHANNELFLOW")
}

ChannelFlow::ChannelFlow()
: d_activeFlow(0)
{
}

ChannelFlow::ChannelFlow(bool activeFlow)
: d_activeFlow(activeFlow)
{
}

bool ChannelFlow::decode(ChannelFlow* flow,
                         const uint8_t* data,
                         bsl::size_t dataLength)
{
    if (dataLength < sizeof(uint8_t)) {
        BALL_LOG_ERROR << "Not enough data to decode ChannelFlow method";
        return false;
    }

    rmqamqpt::Buffer buffer(data, dataLength);
    flow->d_activeFlow = buffer.copy<uint8_t>();
    return true;
}

void ChannelFlow::encode(Writer& output, const ChannelFlow& flow)
{
    Types::write(output, flow.activeFlow());
}

bsl::ostream& operator<<(bsl::ostream& os, const ChannelFlow& flowMethod)
{
    os << "Channel Flow = ["
       << "activeFlow: " << static_cast<int>(flowMethod.activeFlow()) << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
