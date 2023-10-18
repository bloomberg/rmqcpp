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

#ifndef INCLUDED_RMQAMQPT_CONFIRMMETHOD
#define INCLUDED_RMQAMQPT_CONFIRMMETHOD

#include <rmqamqpt_confirmselect.h>
#include <rmqamqpt_confirmselectok.h>

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bdlb_variant.h>
#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

typedef bdlb::Variant<ConfirmSelect, ConfirmSelectOk> SupportedConfirmMethods;

/// \brief Represent an AMQP Confirm class
/// https://www.rabbitmq.com/amqp-0-9-1-reference.html#class.confirm

class ConfirmMethod : public SupportedConfirmMethods {
  public:
    static const rmqamqpt::Constants::AMQPClassId CLASS_ID =
        rmqamqpt::Constants::CONFIRM;

  public:
    /// Initialise internal `bdlb::Variant` with passed ConfirmMethod
    /// This constructor is intentionally implicit.
    template <typename T>
    ConfirmMethod(const T& confirmMethod);

    /// Construct an unset ConfirmMethod variant
    ConfirmMethod();

    /// Fetch the METHOD_ID for the method stored in the Variant. Returns zero
    /// for an unset ConfirmMethod.
    rmqamqpt::Constants::AMQPMethodId methodId() const;

    size_t encodedSize() const;

    /// \brief Encode/Decode a ConfirmMethod to/from network wire format
    class Util {
      public:
        static bool decode(ConfirmMethod* confirmMethod,
                           const uint8_t* data,
                           bsl::size_t dataLength);
        static void encode(Writer& output, const ConfirmMethod& confirmMethod);
    };
};

template <typename T>
ConfirmMethod::ConfirmMethod(const T& confirmMethod)
: SupportedConfirmMethods(confirmMethod)
{
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
