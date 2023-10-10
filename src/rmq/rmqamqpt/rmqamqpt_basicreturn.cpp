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

#include <rmqamqpt_basicreturn.h>

#include <rmqamqpt_types.h>

#include <rmqamqpt_buffer.h>

#include <bdlb_bigendian.h>

#include <bsl_cstddef.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqamqpt {

BasicReturn::BasicReturn(const rmqamqpt::Constants::AMQPReplyCode replyCode,
                         const bsl::string& replyText,
                         const bsl::string& exchange,
                         const bsl::string& routingKey)
: d_replyCode(replyCode)
, d_replyText(replyText)
, d_exchange(exchange)
, d_routingKey(routingKey)
{
}

BasicReturn::BasicReturn()
: d_replyCode()
, d_replyText()
, d_exchange()
, d_routingKey()
{
}

bool BasicReturn::decode(BasicReturn* basicReturn,
                         const uint8_t* data,
                         bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (buffer.available() < sizeof(basicReturn->d_replyCode)) {
        return false;
    }

    const uint16_t replyCode = buffer.copy<bdlb::BigEndianUint16>();
    basicReturn->d_replyCode =
        static_cast<rmqamqpt::Constants::AMQPReplyCode>(replyCode);

    return Types::decodeShortString(&basicReturn->d_replyText, &buffer) &&
           Types::decodeShortString(&basicReturn->d_exchange, &buffer) &&
           Types::decodeShortString(&basicReturn->d_routingKey, &buffer);
}

void BasicReturn::encode(Writer& output, const BasicReturn& basicReturn)
{
    Types::write(output, bdlb::BigEndianUint16::make(basicReturn.d_replyCode));

    Types::encodeShortString(output, basicReturn.d_replyText);
    Types::encodeShortString(output, basicReturn.d_exchange);
    Types::encodeShortString(output, basicReturn.d_routingKey);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicReturn& basicReturn)
{
    return os << "BasicReturn: [reply-code: " << basicReturn.replyCode()
              << ", reply-text: " << basicReturn.replyText()
              << ", exchange: " << basicReturn.exchange()
              << ", routing-key: " << basicReturn.routingKey() << "]";
}

bool operator==(const BasicReturn& lhs, const BasicReturn& rhs)
{
    return (&lhs == &rhs) || (lhs.replyCode() == rhs.replyCode() &&
                              lhs.replyText() == rhs.replyText() &&
                              lhs.exchange() == rhs.exchange() &&
                              lhs.routingKey() == rhs.routingKey());
}

} // namespace rmqamqpt
} // namespace BloombergLP
