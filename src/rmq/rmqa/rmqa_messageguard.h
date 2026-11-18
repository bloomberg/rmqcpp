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

#ifndef INCLUDED_RMQA_MESSAGEGUARD
#define INCLUDED_RMQA_MESSAGEGUARD

#include <rmqp_messageguard.h>
#include <rmqt_consumerack.h>
#include <rmqt_envelope.h>
#include <rmqt_message.h>

#include <bsls_keyword.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bslma_managedptr.h>

namespace BloombergLP {
namespace rmqp {
class Consumer;
}
namespace rmqa {

/// \brief Controls acknowledgements passed to broker for consumed messages
///
/// A MessageGuard is passed to the rmqa::Consumer callback function
/// (defined in rmqp::Consumer) for each message. The object allows
/// acknowledging either positively or negatively, with options to requeue. If a
/// MessageGuard object is allowed to destruct before ack/nack/reject is called
/// - for example because the consumer callback threw - a reject with requeue is
/// automatically signaled to the broker. The delivery is therefore counted as a
/// failed attempt and is subject to the queue's delivery-limit, if one is set.

class MessageGuard : public rmqp::MessageGuard {
  public:
    class Factory {
      public:
        virtual bslma::ManagedPtr<rmqa::MessageGuard>
        create(const rmqt::Message& message,
               const rmqt::Envelope& envelope,
               const MessageGuardCallback& ackCallback,
               rmqp::Consumer* consumer) const;

        virtual ~Factory();
    };

    /// Constructs a new valid guard
    /// \param message Consumed message
    /// \param envelope Consumed delivery metadata
    /// \param ackCallback Callback called when resolving message
    /// \param consumer Pointer to the Consumer
    MessageGuard(const rmqt::Message& message,
                 const rmqt::Envelope& envelope,
                 const MessageGuardCallback& ackCallback,
                 rmqp::Consumer* consumer);

    /// During copying `obj` is invalidated and thereafter cannot be used
    /// to (n)ack.
    MessageGuard(const MessageGuard& obj);

    /// If not processed (`ack()`/`nack()`/`reject()` called), message will be
    /// rejected with requeue automatically.
    ~MessageGuard() BSLS_KEYWORD_OVERRIDE;

    /// Access the received message
    const rmqt::Message& message() const BSLS_KEYWORD_OVERRIDE;

    /// Access the received message envelope (delivery details)
    const rmqt::Envelope& envelope() const BSLS_KEYWORD_OVERRIDE;

    /// Acknowledge the received message. Callable only once.
    /// Should only be called _after_ the message has been fully processed
    ///
    /// A message acknowledgement is fire-and-forget. If the broker does not
    /// receive this (e.g. due to connection drop), the message will be
    /// redelivered to another consumer.
    void ack() BSLS_KEYWORD_OVERRIDE;

    /// Return the received message to the broker without counting this
    /// delivery as a failed processing attempt (`basic.nack`). Callable only
    /// once.
    /// \param requeue `true`
    /// requeues the message on the broker for redelivery , if
    /// `false`, then the message will be either dead lettered (if a
    /// dead letter exchange is specified) or dropped if not.
    ///
    /// \warning Consider `reject` instead. From RabbitMQ 4.3 a quorum queue's
    ///          delivery-limit is measured against the broker's
    ///          delivery-count, which `nack` does not increment, so a message
    ///          returned with `nack(true)` never reaches the limit. See the
    ///          documentation below.
    ///
    /// \see https://www.rabbitmq.com/docs/nack
    /// \see
    /// https://www.rabbitmq.com/docs/quorum-queues#when-is-delivery-count-incremented
    void nack(bool requeue = true) BSLS_KEYWORD_OVERRIDE;

    /// Reject the received message, counting this delivery as a failed
    /// processing attempt (`basic.reject`). Callable only once.
    ///
    /// Use this when processing a message failed. Unlike `nack`, it
    /// increments the broker's delivery-count, so from RabbitMQ 4.3 the
    /// delivery counts towards a quorum queue's delivery-limit, where one is
    /// set, and the message is eventually dead lettered or dropped rather
    /// than redelivered forever.
    ///
    /// \param requeue `true` requeues the message on the broker for
    /// redelivery. If `false`, the message will be either dead lettered (if a
    /// dead letter exchange is specified) or dropped if not.
    ///
    /// \see https://www.rabbitmq.com/docs/nack
    /// \see
    /// https://www.rabbitmq.com/docs/quorum-queues#when-is-delivery-count-incremented
    void reject(bool requeue = true) BSLS_KEYWORD_OVERRIDE;

    /// Pointer to the Consumer, e.g. to cancel the message flow
    virtual rmqp::Consumer* consumer() const BSLS_KEYWORD_OVERRIDE;

    /// Transfers ownership of the message processing to a MessageGuard that is
    /// copyable (shared pointer semantics)
    /// RETURN a shared pointer to a valid message guard, \note if the
    ///        MessageGuard has already been resolved or transferred (ack/nack)
    ///        a warning will be printed
    rmqp::TransferrableMessageGuard transferOwnership() BSLS_KEYWORD_OVERRIDE;

  protected:
    /// Ready - not resolved yet
    /// Invalid - ownership moved
    /// Resolved - message (n)acked.
    enum State { READY, TRANSFERRED, RESOLVED };

    State state() const { return d_state; }

  private:
    mutable State d_state;

    rmqt::Message d_message;
    rmqt::Envelope d_envelope;
    MessageGuardCallback d_ackCallback;
    rmqp::Consumer* d_consumer;

  private:
    void resolve(rmqt::ConsumerAck::Type ackOption);

    /// Assigning guard invalidates the original.
    MessageGuard& operator=(const MessageGuard& other);

    friend bsl::ostream& operator<<(bsl::ostream& os, const MessageGuard& mg);
}; // class MessageGuard

///  Stream out
bsl::ostream& operator<<(bsl::ostream& os, const MessageGuard& mg);

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_MESSAGEGUARD
