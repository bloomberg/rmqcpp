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

#ifndef INCLUDED_RMQAMQPT_EXCHANGEMETHOD
#define INCLUDED_RMQAMQPT_EXCHANGEMETHOD

#include <rmqamqpt_exchangebind.h>
#include <rmqamqpt_exchangebindok.h>
#include <rmqamqpt_exchangedeclare.h>
#include <rmqamqpt_exchangedeclareok.h>

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bdlb_variant.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Represent an AMQP Exchange class
/// https://www.rabbitmq.com/amqp-0-9-1-reference.html#class.exchange

typedef bdlb::
    Variant<ExchangeBind, ExchangeBindOk, ExchangeDeclare, ExchangeDeclareOk>
        SupportedExchangeMethods;
class ExchangeMethod : public SupportedExchangeMethods {
  public:
    static const rmqamqpt::Constants::AMQPClassId CLASS_ID =
        rmqamqpt::Constants::EXCHANGE;

  public:
    /// Initialise internal `bdlb::Variant` with passed exchangeMethod
    /// This constructor is intentionally implicit.
    template <typename T>
    ExchangeMethod(const T& exchangeMethod);

    /// Construct an unset ExchangeMethod variant
    ExchangeMethod();

    /// Fetch the METHOD_ID for the method stored in the Variant. Returns zero
    /// for an unset ExchangeMethod.
    rmqamqpt::Constants::AMQPMethodId methodId() const;

    size_t encodedSize() const;

    /// \brief Encode/Decode a ExchangeMethod to/from network wire format
    class Util {
      public:
        static bool decode(ExchangeMethod* exchangeMethod,
                           const uint8_t* data,
                           bsl::size_t dataLength);
        static void encode(Writer& output,
                           const ExchangeMethod& exchangeMethod);
    };
};

template <typename T>
ExchangeMethod::ExchangeMethod(const T& exchangeMethod)
: SupportedExchangeMethods(exchangeMethod)
{
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
