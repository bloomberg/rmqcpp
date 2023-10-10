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

#ifndef INCLUDED_RMQAMQPT_CONNECTIONOPENOK
#define INCLUDED_RMQAMQPT_CONNECTIONOPENOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

///  \brief Provide connection OPEN-OK method
///
/// This method signals to the client that the connection is ready for use.

class ConnectionOpenOk {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CONNECTION_OPENOK;

    size_t encodedSize() const { return sizeof(uint8_t); }

    static bool decode(ConnectionOpenOk* openOk,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const ConnectionOpenOk& openOk);
};

bsl::ostream& operator<<(bsl::ostream& os,
                         const ConnectionOpenOk& openOkMethod);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
