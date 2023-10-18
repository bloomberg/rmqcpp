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

#ifndef INCLUDED_RMQAMQPT_CONNECTIONSTARTOK
#define INCLUDED_RMQAMQPT_CONNECTIONSTARTOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>

#include <bsl_cstdlib.h>
#include <bsl_iosfwd.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide connection START-OK method
///
/// This method selects a SASL security mechanism.

class ConnectionStartOk {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CONNECTION_STARTOK;

    ConnectionStartOk();

    ConnectionStartOk(const rmqt::FieldTable& properties,
                      const bsl::string& mechanism,
                      const bsl::string& response,
                      const bsl::string& locale);

    bsl::size_t encodedSize() const
    {
        return 2 * sizeof(uint32_t) + 2 * sizeof(uint8_t) +
               FieldValueUtil::encodedTableSize(d_properties) +
               d_mechanism.size() + d_response.size() + d_locale.size();
    }

    const rmqt::FieldTable& properties() const { return d_properties; }

    rmqt::FieldTable& properties() { return d_properties; }

    const bsl::string& mechanism() const { return d_mechanism; }

    const bsl::string& response() const { return d_response; }

    const bsl::string& locale() const { return d_locale; }

    static bool decode(ConnectionStartOk* startOk,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const ConnectionStartOk& startOk);

  private:
    rmqt::FieldTable d_properties;
    bsl::string d_mechanism;
    bsl::string d_response;
    bsl::string d_locale;
};

bsl::ostream& operator<<(bsl::ostream& os,
                         const ConnectionStartOk& startOkMethod);

bool operator==(const ConnectionStartOk& lhs, const ConnectionStartOk& rhs);

inline bool operator!=(const ConnectionStartOk& lhs,
                       const ConnectionStartOk& rhs)
{
    return !(lhs == rhs);
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
