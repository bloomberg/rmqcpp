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

#ifndef INCLUDED_RMQAMQPT_CHANNELFLOWOK
#define INCLUDED_RMQAMQPT_CHANNELFLOWOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide channel FLOW-OK method
///
/// Confirms to the peer that a flow command was received and processed.

class ChannelFlowOk {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CHANNEL_FLOWOK;

    ChannelFlowOk();

    explicit ChannelFlowOk(bool activeFlow);

    size_t encodedSize() const { return sizeof(uint8_t); }

    bool activeFlow() const { return d_activeFlow; }

    static bool
    decode(ChannelFlowOk* flowOk, const uint8_t* data, bsl::size_t dataLength);

    static void encode(Writer& output, const ChannelFlowOk& flowOk);

  private:
    // Bits are packed into octets
    // https://www.rabbitmq.com/resources/specs/amqp0-9-1.pdf (Ref 2.3.2)
    uint8_t d_activeFlow;
};

bsl::ostream& operator<<(bsl::ostream& os, const ChannelFlowOk& flowOkMethod);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
