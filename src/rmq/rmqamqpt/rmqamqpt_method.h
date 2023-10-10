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

#ifndef INCLUDED_RMQAMQPT_METHOD
#define INCLUDED_RMQAMQPT_METHOD

#include <rmqamqpt_basicmethod.h>
#include <rmqamqpt_channelmethod.h>
#include <rmqamqpt_confirmmethod.h>
#include <rmqamqpt_connectionmethod.h>
#include <rmqamqpt_exchangemethod.h>
#include <rmqamqpt_queuemethod.h>

#include <bdlb_variant.h>

#include <bsl_cstdlib.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

typedef bdlb::Variant<BasicMethod,
                      ChannelMethod,
                      ConfirmMethod,
                      ConnectionMethod,
                      ExchangeMethod,
                      QueueMethod>
    SupportedMethods;

/// \brief Represent an AMQP Method decoded from a Frame
///
///  A Variant of different Method class-ids
///  https://www.rabbitmq.com/amqp-0-9-1-reference.html#classes

class Method : public SupportedMethods {
  public:
    /// Constructs an unset Method
    Method() {}

    /// Constructs a Method with the passed Class-Method type
    template <typename T>
    Method(const T& method);

    /// Returns the CLASS_ID of the stored variant
    /// Returns 0 if the Variant is unset.
    rmqamqpt::Constants::AMQPClassId classId() const;

    size_t encodedSize() const;

    /// \brief Encode/Decode a Method to/from network wire format
    class Util {
      public:
        static bool
        decode(Method* method, const uint8_t* buffer, bsl::size_t bufferLen);
        static void encode(Writer& output, const Method& method);

        static bool typeMatch(const Method& lhs, const Method& rhs);
    };
};

template <typename T>
Method::Method(const T& method)
: SupportedMethods(method)
{
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
