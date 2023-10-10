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

#include <rmqamqpt_connectionstart.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <ball_log.h>

#include <bsl_sstream.h>

namespace BloombergLP {
namespace rmqamqpt {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.CONNECTIONSTART")

template <typename ITERATOR>
void joinStrings(bsl::ostringstream& oss,
                 ITERATOR first,
                 ITERATOR last,
                 const bsl::string& delimiter)
{
    if (first != last) {
        oss << *first++;
    }

    for (; first != last; ++first) {
        oss << delimiter << *first;
    }
}
} // namespace

ConnectionStart::ConnectionStart()
: d_versionMajor()
, d_versionMinor()
, d_properties()
, d_mechanisms()
, d_locales()
{
}

ConnectionStart::ConnectionStart(bsl::uint8_t versionMajor,
                                 bsl::uint8_t versionMinor,
                                 const rmqt::FieldTable& properties,
                                 const bsl::vector<bsl::string>& mechanisms,
                                 const bsl::vector<bsl::string>& locales)
: d_versionMajor(versionMajor)
, d_versionMinor(versionMinor)
, d_properties(properties)
, d_mechanisms()
, d_locales()
{
    bsl::ostringstream oss;
    joinStrings(oss, locales.begin(), locales.end(), " ");
    d_locales = oss.str();
    oss.clear();
    joinStrings(oss, mechanisms.begin(), mechanisms.end(), " ");
    d_mechanisms = oss.str();
}

bool ConnectionStart::decode(ConnectionStart* start,
                             const uint8_t* data,
                             bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (buffer.available() < 2) {
        BALL_LOG_ERROR << "Cannot decode ConnectionStart version";
        return false;
    }

    start->d_versionMajor = buffer.copy<bsl::uint8_t>();
    start->d_versionMinor = buffer.copy<bsl::uint8_t>();

    if (!Types::decodeFieldTable(&start->d_properties, &buffer)) {
        BALL_LOG_ERROR << "Cannot decode ConnectionStart properties";
        return false;
    }

    return Types::decodeLongString(&start->d_mechanisms, &buffer) &&
           Types::decodeLongString(&start->d_locales, &buffer);
}

void ConnectionStart::encode(Writer& output, const ConnectionStart& start)
{
    Types::write(output, start.versionMajor());
    Types::write(output, start.versionMinor());

    Types::encodeFieldTable(output, start.properties());

    Types::encodeLongString(output, start.mechanisms());
    Types::encodeLongString(output, start.locales());
}

bsl::ostream& operator<<(bsl::ostream& os, const ConnectionStart& startMethod)
{
    os << "Connection Start = [version:"
       << static_cast<int>(startMethod.versionMajor()) << "."
       << static_cast<int>(startMethod.versionMinor())
       << ", properties:" << startMethod.properties()
       << ", mechanisms:" << startMethod.mechanisms()
       << ", locale:" << startMethod.locales() << "]";
    return os;
}

bool operator==(const ConnectionStart& lhs, const ConnectionStart& rhs)
{
    if (lhs.versionMajor() != rhs.versionMajor()) {
        return false;
    }
    if (lhs.versionMinor() != rhs.versionMinor()) {
        return false;
    }
    if (lhs.locales() != rhs.locales()) {
        return false;
    }
    if (lhs.mechanisms() != rhs.mechanisms()) {
        return false;
    }
    if (lhs.properties() != rhs.properties()) {
        return false;
    }
    return true;
}

} // namespace rmqamqpt
} // namespace BloombergLP
