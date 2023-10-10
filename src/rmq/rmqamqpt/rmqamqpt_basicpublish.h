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

#ifndef INCLUDED_RMQAMQPT_BASICPUBLISH_H
#define INCLUDED_RMQAMQPT_BASICPUBLISH_H

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

///  \brief Provide basic PUBLISH method
///
/// This method publishes a message to a specific exchange. The message will be
/// routed to queues as defined by the exchange configuration and distributed to
/// any active consumers when the transaction, if any, is committed.

class BasicPublish {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_PUBLISH;

    BasicPublish(const bsl::string& exchangeName,
                 const bsl::string& routingKey,
                 bool mandatory,
                 bool immediate);

    BasicPublish();

    size_t encodedSize() const
    {
        return sizeof(uint16_t) + 3 * sizeof(uint8_t) + d_exchangeName.size() +
               d_routingKey.size();
    }

    // Specifies the name of the exchange to publish to. The exchange name can
    // be empty, meaning the default exchange.
    const bsl::string& exchangeName() const { return d_exchangeName; }

    // Specifies the routing key for the message. The routing key is used for
    // routing messages depending on the exchange configuration.
    const bsl::string& routingKey() const { return d_routingKey; }

    // This flag tells the server how to react if the message cannot be routed
    // to a queue. If this flag is set, the server will return an unroutable
    // message with a Return method. If this flag is zero, the server silently
    // drops the message.
    bool mandatory() const { return d_mandatory; }

    // This flag tells the server how to react if the message cannot be routed
    // to a queue consumer immediately. If this flag is set, the server will
    // return an undeliverable message with a Return method. If this flag is
    // zero, the server will queue the message, but with no guarantee that it
    // will ever be consumed.
    bool immediate() const { return d_immediate; }

    static bool decode(BasicPublish*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicPublish&);

  private:
    bsl::string d_exchangeName;
    bsl::string d_routingKey;
    bool d_mandatory;
    bool d_immediate;
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicPublish&);
bool operator==(const BasicPublish&, const BasicPublish&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
