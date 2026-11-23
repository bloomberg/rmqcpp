// Copyright 2026 Bloomberg Finance L.P.
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

#ifndef INCLUDED_RMQAMQPT_BASICREJECT
#define INCLUDED_RMQAMQPT_BASICREJECT

#include <rmqamqpt_constants.h>
#include <rmqamqpt_types.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide basic REJECT method
///
/// This method allows a client to reject a single incoming message. It signals
/// to the broker that processing the message failed, and is the AMQP 0-9-1
/// method for returning an untreatable message to its original queue.
///
/// Unlike BasicNack, this method carries no `multiple` flag, so each rejected
/// delivery requires its own frame.
///
/// Brokers which implement poison message handling count a reject as a failed
/// delivery attempt, where a nack is not counted.

class BasicReject {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_REJECT;

    explicit BasicReject(bsl::uint64_t dt, bool requeue = true);
    BasicReject();

    size_t encodedSize() const { return sizeof(uint64_t) + sizeof(uint8_t); }

    /// The server-assigned and channel-specific delivery tag
    /// The delivery tag is valid only within the channel from which the message
    /// was received. I.e. a client MUST NOT receive a message on one channel
    /// and then acknowledge it on another. The server MUST NOT use a zero value
    /// for delivery tags. Zero is reserved for client use, meaning "all
    /// messages so far received".
    bsl::uint64_t deliveryTag() const { return d_deliveryTag; }

    /// If requeue is true, the server will attempt to requeue the message. If
    /// requeue is false or the requeue attempt fails the messages are discarded
    /// or dead-lettered.
    bool requeue() const { return d_requeue; }

    static bool decode(BasicReject*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicReject&);

  private:
    Types::DeliveryTag d_deliveryTag;
    bool d_requeue;
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicReject&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
