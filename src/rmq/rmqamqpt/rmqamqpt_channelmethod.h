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

#ifndef INCLUDED_RMQAMQPT_CHANNELMETHOD
#define INCLUDED_RMQAMQPT_CHANNELMETHOD

#include <rmqamqpt_channelclose.h>
#include <rmqamqpt_channelcloseok.h>
#include <rmqamqpt_channelflow.h>
#include <rmqamqpt_channelflowok.h>
#include <rmqamqpt_channelopen.h>
#include <rmqamqpt_channelopenok.h>

#include <rmqamqpt_constants.h>

#include <bdlb_variant.h>
#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

typedef bdlb::Variant<ChannelOpen,
                      ChannelOpenOk,
                      ChannelFlow,
                      ChannelFlowOk,
                      ChannelClose,
                      ChannelCloseOk>
    SupportedChannelMethods;

/// \brief Represent an AMQP Channel class:
/// https://www.rabbitmq.com/amqp-0-9-1-reference.html#class.channel

class ChannelMethod : public SupportedChannelMethods {
  public:
    static const rmqamqpt::Constants::AMQPClassId CLASS_ID =
        rmqamqpt::Constants::CHANNEL;

  public:
    /// Initialise internal `bdlb::Variant` with passed ChannelMethod
    /// This constructor is intentionally implicit.
    template <typename T>
    ChannelMethod(const T& channelMethod);

    /// Construct an unset ChannelMethod variant
    ChannelMethod();

    /// Fetch the METHOD_ID for the method stored in the Variant. Returns zero
    /// for an unset ChannelMethod.
    rmqamqpt::Constants::AMQPMethodId methodId() const;

    size_t encodedSize() const;

    /// \brief Encode/Decode a ChannelMethod to/from network wire format

    class Util {
      public:
        static bool decode(ChannelMethod* channelMethod,
                           const uint8_t* data,
                           bsl::size_t dataLength);
        static void encode(Writer& output, const ChannelMethod& channelMethod);
    };
};

template <typename T>
ChannelMethod::ChannelMethod(const T& channelMethod)
: SupportedChannelMethods(channelMethod)
{
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
