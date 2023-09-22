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

#ifndef INCLUDED_RMQAMQPT_BASICACK
#define INCLUDED_RMQAMQPT_BASICACK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_types.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide basic ACK method
///
/// When sent by the client, this method acknowledges one or more messages
/// delivered via the Deliver or Get-Ok methods.
///
/// When sent by server, this method acknowledges one or more messages published
/// with the Publish method on a channel in confirm mode. The acknowledgement
/// can be for a single message or a set of messages up to and including a
/// specific message.

class BasicAck {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_ACK;

    BasicAck(bsl::uint64_t dt, bool multiple);
    BasicAck();

    size_t encodedSize() const { return sizeof(uint64_t) + sizeof(uint8_t); }

    /// The server-assigned and channel-specific delivery tag
    /// The delivery tag is valid only within the channel from which the message
    /// was received. I.e. a client MUST NOT receive a message on one channel
    /// and then acknowledge it on another. The server MUST NOT use a zero value
    /// for delivery tags. Zero is reserved for client use, meaning "all
    /// messages so far received".
    bsl::uint64_t deliveryTag() const { return d_deliveryTag; }

    /// If set to 1, the delivery tag is treated as "up to and including", so
    /// that multiple messages can be acknowledged with a single method. If set
    /// to zero, the delivery tag refers to a single message. If the multiple
    /// field is 1, and the delivery tag is zero, this indicates acknowledgement
    /// of all outstanding messages.
    ///
    /// A message MUST not be acknowledged more than once. The receiving peer
    /// MUST validate that a non-zero delivery-tag refers to a delivered
    /// message, and raise a channel exception if this is not the case. On a
    /// transacted channel, this check MUST be done immediately and not delayed
    /// until a Tx.Commit. Error code: precondition-failed
    bool multiple() const { return d_multiple; }

    static bool decode(BasicAck*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicAck&);

  private:
    Types::DeliveryTag d_deliveryTag;
    bool d_multiple;
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicAck&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
