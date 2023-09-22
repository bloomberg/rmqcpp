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

#ifndef INCLUDED_RMQAMQPT_BASICDELIVER
#define INCLUDED_RMQAMQPT_BASICDELIVER

#include <rmqamqpt_constants.h>
#include <rmqamqpt_types.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

class BasicDeliver {
    /* This method delivers a message to the client, via a consumer. In the
     * asynchronous message delivery model, the client starts a consumer using
     * the Consume method, then the server responds with Deliver methods as and
     * when messages arrive for that consumer.*/
  private:
    Types::ConsumerTag d_consumerTag;
    Types::DeliveryTag d_deliveryTag;
    bool d_redelivered;
    bsl::string d_exchange;
    bsl::string d_routingKey;

  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_DELIVER;

    BasicDeliver(const bsl::string& consumerTag,
                 uint64_t deliveryTag,
                 bool redelivered,
                 const bsl::string& exchangeName,
                 const bsl::string& routingKey);

    BasicDeliver();

    size_t encodedSize() const
    {
        return 4 * sizeof(uint8_t) + sizeof(uint64_t) + d_consumerTag.size() +
               d_exchange.size() + d_routingKey.size();
    }

    const bsl::string& consumerTag() const { return d_consumerTag; }

    bsl::uint64_t deliveryTag() const { return d_deliveryTag; }

    // Specifies the routing key name specified when the message was published.
    const bsl::string& routingKey() const { return d_routingKey; }

    // This indicates that the message has been previously delivered to this or
    // another client.
    bool redelivered() const { return d_redelivered; }

    // Specifies the name of the exchange that the message was originally
    // published to. May be empty, indicating the default exchange.
    const bsl::string& exchange() const { return d_exchange; }

    static bool decode(BasicDeliver*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicDeliver&);
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicDeliver&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
