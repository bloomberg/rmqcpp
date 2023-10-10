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
        virtual ~Factory();
        virtual bsl::shared_ptr<ProducerImpl>
        create(uint16_t maxOutstandingConfirms,
               const rmqt::ExchangeHandle& exchange,
               const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
               bdlmt::ThreadPool& threadPool,
               rmqio::EventLoop& eventLoop) const;
    };

    // CREATORS
    ProducerImpl(uint16_t maxOutstandingConfirms,
                 const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
                 bdlmt::ThreadPool& threadPool,
                 rmqio::EventLoop& eventLoop);

    ~ProducerImpl() BSLS_KEYWORD_OVERRIDE;

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

    rmqio::EventLoop& d_eventLoop;

    bsl::shared_ptr<rmqamqp::SendChannel> d_channel;

    bsl::shared_ptr<SharedState> d_sharedState;

}; // class Producer

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_PRODUCER
