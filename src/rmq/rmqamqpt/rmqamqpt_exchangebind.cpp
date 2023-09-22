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

#include <rmqamqpt_exchangebind.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bdlb_bigendian.h>

#include <bsl_sstream.h>

namespace BloombergLP {
namespace rmqamqpt {

ExchangeBind::ExchangeBind()
: d_sourceX()
, d_destinationX()
, d_routingKey()
, d_noWait()
, d_arguments()
{
}

ExchangeBind::ExchangeBind(const bsl::string& sourceX,
                           const bsl::string& destinationX,
                           const bsl::string& routingKey,
                           bool noWait,
                           const rmqt::FieldTable& arguments)
: d_sourceX(sourceX)
, d_destinationX(destinationX)
, d_routingKey(routingKey)
, d_noWait(noWait)
, d_arguments(arguments)
{
}

bool ExchangeBind::decode(ExchangeBind* bind,
                          const uint8_t* data,
                          bsl::size_t dataLength)
{

    rmqamqpt::Buffer buffer(data, dataLength);

    // Skip reserved short
    if (buffer.available() < sizeof(uint16_t)) {
        return false;
    }
    buffer.skip(sizeof(uint16_t));

    if (!Types::decodeShortString(&bind->d_destinationX, &buffer)) {
        return false;
    }

    if (!Types::decodeShortString(&bind->d_sourceX, &buffer)) {
        return false;
    }

    if (!Types::decodeShortString(&bind->d_routingKey, &buffer)) {
        return false;
    }

    if (buffer.available() < sizeof(bind->d_noWait)) {
        return false;
    }

    bind->d_noWait = buffer.copy<uint8_t>();

    return Types::decodeFieldTable(&bind->d_arguments, &buffer);
}

void ExchangeBind::encode(Writer& output, const ExchangeBind& bind)
{
    Types::write(output, bdlb::BigEndianUint16::make(0));
    Types::encodeShortString(output, bind.destinationX());
    Types::encodeShortString(output, bind.sourceX());
    Types::encodeShortString(output, bind.routingKey());

    Types::write(output, static_cast<uint8_t>(bind.d_noWait));

    Types::encodeFieldTable(output, bind.arguments());
}

bool operator==(const ExchangeBind& lhs, const ExchangeBind& rhs)
{
    return (&lhs == &rhs) ||
           (lhs.destinationX() == rhs.destinationX() &&
            lhs.sourceX() == rhs.sourceX() &&
            lhs.routingKey() == rhs.routingKey() &&
            lhs.noWait() == rhs.noWait() && lhs.arguments() == rhs.arguments());
}

bsl::ostream& operator<<(bsl::ostream& os, const ExchangeBind& exchangeBind)
{
    os << "ExchangeBind = [source-exchange: " << exchangeBind.sourceX()
       << ", destination-exchange: " << exchangeBind.destinationX()
       << ", routing-key: " << exchangeBind.routingKey()
       << ", no-wait: " << exchangeBind.noWait()
       << ", arguments: " << exchangeBind.arguments() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
