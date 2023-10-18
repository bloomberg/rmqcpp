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

#ifndef INCLUDED_RMQA_TRACINGMESSAGEGUARD
#define INCLUDED_RMQA_TRACINGMESSAGEGUARD

#include <rmqa_messageguard.h>

#include <rmqp_consumertracing.h>

#include <rmqt_consumerack.h>
#include <rmqt_envelope.h>
#include <rmqt_message.h>
#include <rmqt_queue.h>

#include <bsl_memory.h>
#include <bslma_managedptr.h>
#include <bsls_keyword.h>

namespace BloombergLP {
namespace rmqa {

/// \brief Controls acknowledgements passed to broker for consumed messages
///
/// A TracingMessageGuard is passed to the rmqa::Consumer callback function
/// (defined in rmqp::Consumer) for each message in the event that
/// ConsumerTracing is enabled. The difference being that the ConsumerTracing
/// context is created from the thread that will process the message callback,
/// and the context will be bound to the messageGuard. The object allows
/// acknowledging either positively or negatively, with options to requeue. If a
/// MessageGuard object is allowed to destruct before ack/nack is called a nack
/// with requeue is automatically signaled to the broker.

class TracingMessageGuard : public rmqa::MessageGuard {
  public:
    class Factory : public rmqa::MessageGuard::Factory {
      public:
        Factory(const rmqt::QueueHandle& queue,
                const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
                const bsl::shared_ptr<rmqp::ConsumerTracing>& contextFactory);

        virtual bslma::ManagedPtr<rmqa::MessageGuard>
        create(const rmqt::Message& message,
               const rmqt::Envelope& envelope,
               const MessageGuardCallback& ackCallback,
               rmqp::Consumer* consumer) const BSLS_KEYWORD_OVERRIDE;

      private:
        bsl::string d_queueName;
        bsl::shared_ptr<const rmqt::Endpoint> d_endpoint;
        bsl::shared_ptr<rmqp::ConsumerTracing> d_contextFactory;
    };

    /// Transfers ownership of the message processing to a MessageGuard that is
    /// copyable (shared pointer semantics)
    /// RETURN a shared pointer to a valid message guard, \note if the
    ///        MessageGuard has already been resolved or transferred (ack/nack)
    ///        a warning will be printed
    rmqp::TransferrableMessageGuard transferOwnership() BSLS_KEYWORD_OVERRIDE;

    /// Constructs a new valid guard
    /// \param message Consumed message
    /// \param envelope Consumed delivery metadata
    /// \param ackCallback Callback called when resolving message
    /// \param consumer Pointer to the Consumer
    TracingMessageGuard(
        const rmqt::Message& message,
        const rmqt::Envelope& envelope,
        const MessageGuardCallback& ackCallback,
        rmqp::Consumer* consumer,
        const bsl::string& queueName,
        const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
        const bsl::shared_ptr<rmqp::ConsumerTracing>& contextFactory);

    /// During copying `obj` is invalidated and thereafter cannot be used
    /// to (n)ack.
    TracingMessageGuard(const TracingMessageGuard& obj);

  private:
    bsl::shared_ptr<rmqp::ConsumerTracing::Context> d_context;
}; // class TracingMessageGuard

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_TRACINGMESSAGEGUARD
