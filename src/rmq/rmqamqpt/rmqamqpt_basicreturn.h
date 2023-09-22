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

#ifndef INCLUDED_RMQAMQPT_BASICRETURN_H
#define INCLUDED_RMQAMQPT_BASICRETURN_H

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

class BasicReturn {
    /*  Return a failed message.
        This method returns an undeliverable message that was published with the
        "immediate" flag set, or an unroutable message published with the
        "mandatory" flag set. The reply code and text provide information about
        the reason that the message was undeliverable.
    */

    rmqamqpt::Constants::AMQPReplyCode d_replyCode;
    bsl::string d_replyText;
    bsl::string d_exchange;
    bsl::string d_routingKey;

  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_RETURN;

    BasicReturn(const rmqamqpt::Constants::AMQPReplyCode replyCode,
                const bsl::string& replyText,
                const bsl::string& exchange,
                const bsl::string& routingKey);

    BasicReturn();

    size_t encodedSize() const
    {
        return sizeof(uint16_t) + 3 * sizeof(uint8_t) + d_replyText.size() +
               d_exchange.size() + d_routingKey.size();
    }

    bsl::uint16_t replyCode() const { return d_replyCode; }

    const bsl::string& replyText() const { return d_replyText; }

    // Specifies the name of the exchange that the message was originally
    // published to. May be empty, meaning the default exchange.
    const bsl::string& exchange() const { return d_exchange; }

    // Specifies the routing key name specified when the message was published.
    const bsl::string& routingKey() const { return d_routingKey; }

    static bool decode(BasicReturn*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicReturn&);
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicReturn&);
bool operator==(const BasicReturn&, const BasicReturn&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
