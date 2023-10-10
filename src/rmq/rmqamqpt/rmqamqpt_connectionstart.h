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

#ifndef INCLUDED_RMQAMQPT_CONNECTIONSTART
#define INCLUDED_RMQAMQPT_CONNECTIONSTART

#include <rmqamqpt_constants.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide connection START method
///
/// This method starts the connection negotiation process by telling the client
/// the protocol version that the server proposes, along with a list of security
/// mechanisms which the client can use for authentication.

class ConnectionStart {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CONNECTION_START;

    ConnectionStart();

    ConnectionStart(bsl::uint8_t versionMajor,
                    bsl::uint8_t versionMinor,
                    const rmqt::FieldTable& properties,
                    const bsl::vector<bsl::string>& mechanisms,
                    const bsl::vector<bsl::string>& locales);

    bsl::size_t encodedSize() const
    {
        return 2 * sizeof(uint8_t) + 3 * sizeof(uint32_t) +
               FieldValueUtil::encodedTableSize(d_properties) +
               d_mechanisms.size() + d_locales.size();
    }

    const rmqt::FieldTable& properties() const { return d_properties; }

    const bsl::string& mechanisms() const { return d_mechanisms; }

    const bsl::string& locales() const { return d_locales; }

    bsl::uint8_t versionMajor() const { return d_versionMajor; }

    bsl::uint8_t versionMinor() const { return d_versionMinor; }

    static bool
    decode(ConnectionStart* start, const uint8_t* data, bsl::size_t dataLength);
    static void encode(Writer& output, const ConnectionStart& start);

  private:
    bsl::uint8_t d_versionMajor;
    bsl::uint8_t d_versionMinor;
    rmqt::FieldTable d_properties;
    bsl::string d_mechanisms;
    bsl::string d_locales;
};

bsl::ostream& operator<<(bsl::ostream& os, const ConnectionStart& startMethod);

bool operator==(const ConnectionStart& lhs, const ConnectionStart& rhs);

inline bool operator!=(const ConnectionStart& lhs, const ConnectionStart& rhs)
{
    return !(lhs == rhs);
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
