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

#ifndef INCLUDED_RMQAMQPT_CONNECTIONCLOSEOK
#define INCLUDED_RMQAMQPT_CONNECTIONCLOSEOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide connection CLOSE-OK method
///
/// This method confirms a Connection.Close method and tells the recipient that
/// it is safe to release resources for the connection and close the socket.

class ConnectionCloseOk {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CONNECTION_CLOSEOK;

    size_t encodedSize() const { return 0; }

    static bool decode(ConnectionCloseOk*, const uint8_t*, bsl::size_t)
    {
        // nothing to decode
        return true;
    }

    static void encode(Writer&, const ConnectionCloseOk&) {}
};

bsl::ostream& operator<<(bsl::ostream& os,
                         const ConnectionCloseOk& closeOkMethod);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
