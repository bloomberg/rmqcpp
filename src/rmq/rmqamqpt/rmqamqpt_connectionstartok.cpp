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

#include <rmqamqpt_connectionstartok.h>

#include <rmqamqpt_types.h>

namespace BloombergLP {
namespace rmqamqpt {

ConnectionStartOk::ConnectionStartOk()
: d_properties()
, d_mechanism()
, d_response()
, d_locale()
{
}

ConnectionStartOk::ConnectionStartOk(const rmqt::FieldTable& properties,
                                     const bsl::string& mechanism,
                                     const bsl::string& response,
                                     const bsl::string& locale)
: d_properties(properties)
, d_mechanism(mechanism)
, d_response(response)
, d_locale(locale)
{
}

bool ConnectionStartOk::decode(ConnectionStartOk* startOk,
                               const uint8_t* data,
                               bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    return Types::decodeFieldTable(&startOk->d_properties, &buffer) &&
           Types::decodeShortString(&startOk->d_mechanism, &buffer) &&
           Types::decodeLongString(&startOk->d_response, &buffer) &&
           Types::decodeShortString(&startOk->d_locale, &buffer);
}

void ConnectionStartOk::encode(Writer& output, const ConnectionStartOk& startOk)
{
    Types::encodeFieldTable(output, startOk.d_properties);
    Types::encodeShortString(output, startOk.d_mechanism);

    Types::encodeLongString(output, startOk.d_response);
    Types::encodeShortString(output, startOk.d_locale);
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const ConnectionStartOk& startOkMethod)
{
    os << "Connection StartOk = [properties:" << startOkMethod.properties()
       << ", mechanism:" << startOkMethod.mechanism()
       << ", response:" << startOkMethod.response()
       << ", locale:" << startOkMethod.locale() << "]";
    return os;
}

bool operator==(const ConnectionStartOk& lhs, const ConnectionStartOk& rhs)
{
    if (lhs.locale() != rhs.locale()) {
        return false;
    }
    if (lhs.mechanism() != rhs.mechanism()) {
        return false;
    }
    if (lhs.response() != rhs.response()) {
        return false;
    }
    if (lhs.properties() != rhs.properties()) {
        return false;
    }
    return true;
}

} // namespace rmqamqpt
} // namespace BloombergLP
