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

#ifndef INCLUDED_RMQAMQPT_BASICMETHOD_H
#define INCLUDED_RMQAMQPT_BASICMETHOD_H

#include <rmqamqpt_basicack.h>
#include <rmqamqpt_basiccancel.h>
#include <rmqamqpt_basiccancelok.h>
#include <rmqamqpt_basicconsume.h>
#include <rmqamqpt_basicconsumeok.h>
#include <rmqamqpt_basicdeliver.h>
#include <rmqamqpt_basicnack.h>
#include <rmqamqpt_basicpublish.h>
#include <rmqamqpt_basicqos.h>
#include <rmqamqpt_basicqosok.h>
#include <rmqamqpt_basicreturn.h>
#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bdlb_variant.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>

namespace BloombergLP {
namespace rmqamqpt {

typedef bdlb::Variant<BasicAck,
                      BasicCancel,
                      BasicCancelOk,
                      BasicConsume,
                      BasicConsumeOk,
                      BasicDeliver,
                      BasicNack,
                      BasicPublish,
                      BasicQoS,
                      BasicQoSOk,
                      BasicReturn>
    SupportedBasicMethods;

/// \brief Represent an AMQP Basic class:
/// https://www.rabbitmq.com/amqp-0-9-1-reference.html#class.

class BasicMethod : public SupportedBasicMethods {
  public:
    static const rmqamqpt::Constants::AMQPClassId CLASS_ID =
        rmqamqpt::Constants::BASIC;

  public:
    /// Initialise internal `bdlb::Variant` with passed connectionMethod
    /// This constructor is intentionally implicit.
    template <typename T>
    BasicMethod(const T& basicMethod);

    /// Construct an unset ConnectionMethod variant
    BasicMethod();

    /// Fetch the METHOD_ID for the method stored in the Variant. Returns zero
    /// for an unset ConnectionMethod.
    rmqamqpt::Constants::AMQPMethodId methodId() const;

    size_t encodedSize() const;

    /// \brief Encode/Decode a BasicMethod to/from network wire format

    class Util {
      public:
        static bool decode(BasicMethod* basicMethod,
                           const uint8_t* data,
                           bsl::size_t dataLength);

        static void encode(Writer& output, const BasicMethod& basicMethod);
    };
};

template <typename T>
BasicMethod::BasicMethod(const T& basicMethod)
: SupportedBasicMethods(basicMethod)
{
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
