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

#ifndef INCLUDED_RMQAMQPT_BASICCONSUME_H
#define INCLUDED_RMQAMQPT_BASICCONSUME_H

#include <rmqamqpt_constants.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

///  \brief Provide basic CONSUME method
///
/// This method asks the server to start a "consumer", which is a transient
/// request for messages from a specific queue. Consumers last as long as the
/// channel they were declared on, or until the client cancels them.

class BasicConsume {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_CONSUME;

    BasicConsume(const bsl::string& queue,
                 const bsl::string& consumerTag,
                 const rmqt::FieldTable& arguments = rmqt::FieldTable(),
                 bool noLocal                      = false,
                 bool noAck                        = false,
                 bool exclusive                    = false,
                 bool noWait                       = false);

    BasicConsume();

    bsl::size_t encodedSize() const
    {
        return sizeof(uint16_t) + 3 * sizeof(uint8_t) + sizeof(uint32_t) +
               d_queue.size() + d_consumerTag.size() +
               FieldValueUtil::encodedTableSize(d_arguments);
    }

    /// queue name
    const bsl::string& queue() const { return d_queue; }

    /// Identifier for the consumer, valid within the current channel.
    const bsl::string& consumerTag() const { return d_consumerTag; }

    /// If the no-local field is set the server will not send messages to the
    /// connection that published them.
    bool noLocal() const { return d_noLocal; }

    /// If this field is set the server does not expect acknowledgements for
    /// messages. That is, when a message is delivered to the client the server
    /// assumes the delivery will succeed and immediately dequeues it. This
    /// functionality may increase performance but at the cost of reliability.
    /// Messages can get lost if a client dies before they are delivered to the
    /// application.
    bool noAck() const { return d_noAck; }

    /// Exclusive queues may only be accessed by the current connection, and
    /// are deleted when that connection closes. Passive declaration of an
    /// exclusive queue by other connections are not allowed.
    bool exclusive() const { return d_exclusive; }

    /// If set, the server will not respond to the method. The client should
    /// not wait for a reply method. If the server could not complete the
    /// method it will raise a channel or connection exception.
    bool noWait() const { return d_noWait; }

    /// A set of arguments for the declaration. The syntax and semantics of
    /// these arguments depends on the server implementation.
    const rmqt::FieldTable& arguments() const { return d_arguments; }

    static bool decode(BasicConsume*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicConsume&);

  private:
    bsl::string d_queue;
    bsl::string d_consumerTag;
    bool d_noLocal;
    bool d_noAck;
    bool d_exclusive;
    bool d_noWait;
    rmqt::FieldTable d_arguments;
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicConsume&);
bool operator==(const BasicConsume&, const BasicConsume&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
