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

#ifndef INCLUDED_RMQA_PRODUCERIMPL
#define INCLUDED_RMQA_PRODUCERIMPL

#include <rmqp_producertagger.h>

#include <rmqp_messagetransformer.h>
#include <rmqp_producer.h>
#include <rmqt_endpoint.h>
#include <rmqt_exchange.h>
#include <rmqt_future.h>
#include <rmqt_message.h>
#include <rmqt_queue.h>
#include <rmqt_result.h>

#include <rmqamqp_sendchannel.h>

#include <bdlb_guid.h>
#include <bdlmt_threadpool.h>
#include <bslma_managedptr.h>
#include <bslmt_mutex.h>
#include <bslmt_timedsemaphore.h>
#include <bsls_keyword.h>
#include <bsls_timeinterval.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_unordered_map.h>
#include <bsl_vector.h>

//@PURPOSE: Implements the rmqa::Producer interface
//
//@CLASSES:
//  rmqa::ProducerImpl: Manages interaction between rmqa <-> rmq internals

namespace BloombergLP {
namespace rmqio {
class EventLoop;
}
namespace rmqa {

class ProducerImpl : public rmqp::Producer {
  public:
    class Factory {
      public:
        /// Create producers with no tagging hook.
        Factory();

        /// Create producers which invoke `tagger` on each outgoing message.
        /// The tagger is shared by every producer this factory creates.
        explicit Factory(const bsl::shared_ptr<rmqp::ProducerTagger>& tagger);

        virtual ~Factory();
        virtual bsl::shared_ptr<ProducerImpl>
        create(uint16_t maxOutstandingConfirms,
               const rmqt::ExchangeHandle& exchange,
               const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
               bdlmt::ThreadPool& threadPool,
               rmqio::EventLoop& eventLoop) const;

      private:
        bsl::shared_ptr<rmqp::ProducerTagger> d_tagger;
    };

    // CREATORS
    ProducerImpl(uint16_t maxOutstandingConfirms,
                 const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
                 bdlmt::ThreadPool& threadPool,
                 rmqio::EventLoop& eventLoop,
                 const bsl::string& exchangeName = bsl::string(),
                 const bsl::shared_ptr<rmqp::ProducerTagger>& tagger =
                     bsl::shared_ptr<rmqp::ProducerTagger>());

    ~ProducerImpl() BSLS_KEYWORD_OVERRIDE;

    void
    addTransformer(const bsl::shared_ptr<rmqp::MessageTransformer>& transformer)
        BSLS_KEYWORD_OVERRIDE;

    SendStatus send(const rmqt::Message& message,
                    const bsl::string& routingKey,
                    const rmqp::Producer::ConfirmationCallback& confirmCallback,
                    const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;

    SendStatus send(const rmqt::Message& message,
                    const bsl::string& routingKey,
                    rmqt::Mandatory::Value mandatoryFlag,
                    const rmqp::Producer::ConfirmationCallback& confirmCallback,
                    const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;

    SendStatus
    trySend(const rmqt::Message& message,
            const bsl::string& routingKey,
            const rmqp::Producer::ConfirmationCallback& confirmCallback)
        BSLS_KEYWORD_OVERRIDE;

    rmqt::Future<> updateTopologyAsync(
        const rmqt::TopologyUpdate& topologyUpdate) BSLS_KEYWORD_OVERRIDE;

    rmqt::Result<>
    waitForConfirms(const bsls::TimeInterval& timeout = bsls::TimeInterval(0))
        BSLS_KEYWORD_OVERRIDE;

    typedef bsl::unordered_map<bdlb::Guid, rmqp::Producer::ConfirmationCallback>
        CallbackMap;

    // State shared with event loop thread
    struct SharedState {
        SharedState(bool _isValid,
                    bdlmt::ThreadPool& _threadPool,
                    uint16_t maxOutstandingConfirms)
        : callbackMap()
        , mutex()
        , isValid(_isValid)
        , threadPool(_threadPool)
        , outstandingMessagesCap(maxOutstandingConfirms)
        , waitForConfirmsFuture()
        {
        }

        // Can only be accessed when mutex is held
        CallbackMap callbackMap;

        bslmt::Mutex mutex;
        bool isValid;
        bdlmt::ThreadPool& threadPool;
        bslmt::TimedSemaphore outstandingMessagesCap;
        bsl::optional<rmqt::Future<>::Pair> waitForConfirmsFuture;
    };

  private:
    ProducerImpl(const ProducerImpl&) BSLS_KEYWORD_DELETED;
    ProducerImpl& operator=(const ProducerImpl&) BSLS_KEYWORD_DELETED;

    bool registerUniqueCallback(
        const bdlb::Guid& guid,
        const rmqp::Producer::ConfirmationCallback& confirmCallback);

    /// Return a copy of `message`, having offered it to the tagger. On return
    /// `*callback` is the callback to publish with, wrapped by the tagger if
    /// it asked to be.
    ///
    /// Must be called on the sending thread and before any wait on the
    /// outstanding confirm limit, as `rmqp::ProducerTagger` requires.
    rmqt::Message
    prepareMessageForSending(rmqp::Producer::ConfirmationCallback* callback,
                             const rmqt::Message& message,
                             const bsl::string& routingKey);

    rmqp::Producer::SendStatus
    doSend(const rmqt::Message& message,
           const bsl::string& routingKey,
           rmqt::Mandatory::Value mandatoryFlag,
           const rmqp::Producer::ConfirmationCallback& confirmCallback);

    rmqp::Producer::SendStatus
    sendImpl(const rmqt::Message& message,
             const bsl::string& routingKey,
             rmqt::Mandatory::Value mandatoryFlag,
             const rmqp::Producer::ConfirmationCallback& confirmCallback,
             const bsls::TimeInterval& timeout);

    bool applyTransformations(rmqt::Message& dstMessage,
                              const rmqt::Message& srcMessage);

    rmqio::EventLoop& d_eventLoop;

    bsl::shared_ptr<rmqamqp::SendChannel> d_channel;

    bsl::shared_ptr<SharedState> d_sharedState;

    bsl::vector<bsl::shared_ptr<rmqp::MessageTransformer> > d_transformers;

    bsl::string d_exchangeName;

    /// Null when nothing is configured to tag outgoing messages.
    bsl::shared_ptr<rmqp::ProducerTagger> d_tagger;

}; // class Producer

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_PRODUCER
