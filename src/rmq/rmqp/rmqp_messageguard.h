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

#ifndef INCLUDED_RMQP_MESSAGEGUARD
#define INCLUDED_RMQP_MESSAGEGUARD

#include <rmqt_consumerack.h>
#include <rmqt_message.h>

#include <bsl_functional.h>
#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqp {
class Consumer;
class MessageGuard;

typedef bsl::shared_ptr<MessageGuard> TransferrableMessageGuard;

/// \brief An interface for MessageGuard class.
///
/// Received messages are passed to consumers with MessageGuard objects. A
/// message guard provides access to the received message and methods for
/// acknowledging (or nack'ing) it. This interface is provided for testing
/// purposes.

class MessageGuard {
  public:
    typedef bsl::function<void(const rmqt::ConsumerAck&)> MessageGuardCallback;

    virtual ~MessageGuard();

    /// Access the received message
    virtual const rmqt::Message& message() const = 0;

    /// Access the received message envelope (delivery details)
    virtual const rmqt::Envelope& envelope() const = 0;

    /// Acknowledge the received message. Callable only once.
    /// Should only be called _after_ the message has been fully processed
    virtual void ack() = 0;

    /// Negative acknowledge the received message. Callable only once.
    /// by default requeues the message on the broker for redelivery, if
    /// requeue is false, then the message will be either dead lettered (if a
    /// dead letter exchange is specified) or dropped if not.
    virtual void nack(bool requeue = true) = 0;

    /// Reference to the Consumer
    virtual Consumer* consumer() const = 0;

    /// Transfers ownership of the message processing to a MessageGuard that is
    /// copyable (shared pointer semantics)
    /// RETURN a shared pointer to a valid message guard, \note if the
    ///        MessageGuard has already been resolved or transferred (ack/nack)
    ///        a warning will be printed
    virtual TransferrableMessageGuard transferOwnership() = 0;

  private:
    MessageGuard& operator=(const MessageGuard& other);

}; // class MessageGuard

} // namespace rmqp
} // namespace BloombergLP

#endif // ! INCLUDED_RMQP_MESSAGEGUARD
