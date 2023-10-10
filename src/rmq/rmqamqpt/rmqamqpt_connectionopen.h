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

#ifndef INCLUDED_RMQAMQPT_CONNECTIONOPEN
#define INCLUDED_RMQAMQPT_CONNECTIONOPEN

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide connection OPEN method
///
/// This method opens a connection to a virtual host, which is a collection of
/// resources, and acts to separate multiple application domains within a
/// server.
///
/// The server may apply arbitrary limits per virtual host, such as the number
/// of each type of entity that may be used, per connection and/or in total.

class ConnectionOpen {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CONNECTION_OPEN;

    size_t encodedSize() const
    {
        return 3 * sizeof(uint8_t) + d_virtualHost.size();
    }

    explicit ConnectionOpen(const bsl::string& virtualHost = "/");

    const bsl::string& virtualHost() const { return d_virtualHost; }

    static bool
    decode(ConnectionOpen* open, const uint8_t* data, bsl::size_t dataLength);
    static void encode(Writer& output, const ConnectionOpen& open);

  private:
    bsl::string d_virtualHost;
};

bsl::ostream& operator<<(bsl::ostream& os, const ConnectionOpen& openMethod);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
