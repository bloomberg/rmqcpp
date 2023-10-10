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

#ifndef INCLUDED_RMQAMQPT_CONNECTIONMETHOD
#define INCLUDED_RMQAMQPT_CONNECTIONMETHOD

#include <rmqamqpt_connectionclose.h>
#include <rmqamqpt_connectioncloseok.h>
#include <rmqamqpt_connectionopen.h>
#include <rmqamqpt_connectionopenok.h>
#include <rmqamqpt_connectionstart.h>
#include <rmqamqpt_connectionstartok.h>
#include <rmqamqpt_connectiontune.h>
#include <rmqamqpt_connectiontuneok.h>
#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

typedef bdlb::Variant<ConnectionClose,
                      ConnectionCloseOk,
                      ConnectionOpen,
                      ConnectionOpenOk,
                      ConnectionStart,
                      ConnectionStartOk,
                      ConnectionTune,
                      ConnectionTuneOk>
    SupportedConnectionMethods;

/// \brief Represents an AMQP Connection class:
/// https://www.rabbitmq.com/amqp-0-9-1-reference.html#class.connection

class ConnectionMethod : public SupportedConnectionMethods {
  public:
    static const rmqamqpt::Constants::AMQPClassId CLASS_ID =
        rmqamqpt::Constants::CONNECTION;

    /// Initialise internal `bdlb::Variant` with passed connectionMethod
    /// This constructor is intentionally implicit.
    template <typename T>
    ConnectionMethod(const T& connectionMethod);

    /// Construct an unset ConnectionMethod variant
    ConnectionMethod();

    /// Fetch the METHOD_ID for the method stored in the Variant. Returns zero
    /// for an unset ConnectionMethod.
    rmqamqpt::Constants::AMQPMethodId methodId() const;

    size_t encodedSize() const;

    /// \brief Encode/Decode a ConnectionMethod to/from network wire format
    class Util {
      public:
        static bool decode(ConnectionMethod* connMethod,
                           const uint8_t* data,
                           bsl::size_t dataLength);
        static void encode(Writer& output, const ConnectionMethod& connMethod);
    };
};

template <typename T>
ConnectionMethod::ConnectionMethod(const T& connectionMethod)
: SupportedConnectionMethods(connectionMethod)
{
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
