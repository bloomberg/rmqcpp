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

#ifndef INCLUDED_RMQA_CONSUMERIMPL
#define INCLUDED_RMQA_CONSUMERIMPL

#include <rmqa_messageguard.h>

#include <rmqio_eventloop.h>
#include <rmqp_consumer.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_endpoint.h>
#include <rmqt_envelope.h>
#include <rmqt_message.h>
#include <rmqt_queue.h>
#include <rmqt_result.h>

#include <bdlmt_threadpool.h>
#include <bslmt_lockguard.h>
#include <bslmt_mutex.h>
#include <bsls_keyword.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_string.h>

//@PURPOSE: Provide a RabbitMQ Async Consumer API
//
//@CLASSES:
//  rmqa::Consumer: a RabbitMQ Async Consumer object

namespace BloombergLP {

namespace rmqamqp {
class ReceiveChannel;
}

namespace rmqa {

class ConsumerImpl : public rmqp::Consumer,
                     public bsl::enable_shared_from_this<ConsumerImpl> {
  public:
    class Factory {
      public:
        virtual ~Factory();
        virtual bsl::shared_ptr<ConsumerImpl>
        create(const bsl::shared_ptr<rmqamqp::ReceiveChannel>& channel,
               rmqt::QueueHandle queue,
               const bsl::shared_ptr<ConsumerFunc>& onMessage,
               const bsl::string& consumerTag,
               bdlmt::ThreadPool& threadPool,
               rmqio::EventLoop& eventLoop,
               const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue) const;
    };

    // CREATORS
    ConsumerImpl(const bsl::shared_ptr<rmqamqp::ReceiveChannel>& channel,
                 rmqt::QueueHandle queue,
                 const bsl::shared_ptr<ConsumerFunc>& onMessage,
                 const bsl::string& consumerTag,
                 bdlmt::ThreadPool& threadPool,
                 rmqio::EventLoop& eventLoop,
                 const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
                 const bsl::shared_ptr<rmqa::MessageGuard::Factory>&
                     messageGuardFactory);

    /// Destructor stops the consumer
    ~ConsumerImpl();

    rmqt::Result<> start();

    /// Cancels the consumer, stops new messages flowing in
    rmqt::Future<> cancel() BSLS_KEYWORD_OVERRIDE;

    /// return a future to wait for consumer to ack all the outstanding
    /// messages
    rmqt::Future<> drain() BSLS_KEYWORD_OVERRIDE;

    rmqt::Result<>
    cancelAndDrain(const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;

    rmqt::Future<> updateTopologyAsync(
        const rmqt::TopologyUpdate& topologyUpdate) BSLS_KEYWORD_OVERRIDE;

  private:
    ConsumerImpl(const ConsumerImpl&) BSLS_KEYWORD_DELETED;
    ConsumerImpl& operator=(const ConsumerImpl&) BSLS_KEYWORD_DELETED;

    void ackMessage(const rmqt::ConsumerAck& ack);

    /// Called from the event loop thread with a received message
    /// This class dispatches this call onto a threadpool thread
    static void
    handleMessage(const bsl::weak_ptr<ConsumerImpl>& consumerWeakPtr,
                  bdlmt::ThreadPool& threadPool,
                  const rmqt::Message& message,
                  const rmqt::Envelope& envelope);

    static void
    threadPoolHandleMessage(const bsl::weak_ptr<ConsumerImpl>& consumer,
                            const rmqt::Message& message,
                            const rmqt::Envelope& envelope);

    static void messageGuardCb(const bsl::weak_ptr<ConsumerImpl>& consumerPtr,
                               const rmqt::ConsumerAck& ack);

  private:
    bsl::string d_consumerTag;
    rmqt::QueueHandle d_queue;
    bsl::shared_ptr<rmqp::Consumer::ConsumerFunc> d_onMessage;
    bdlmt::ThreadPool& d_threadPool;
    rmqio::EventLoop& d_eventLoop;
    bsl::shared_ptr<rmqt::ConsumerAckQueue> d_ackQueue;
    bsl::shared_ptr<rmqt::ConsumerAckBatch> d_ackBatch;
    bslmt::Mutex d_ackMessageMutex;

    bsl::shared_ptr<rmqamqp::ReceiveChannel> d_channel;
    bsl::shared_ptr<MessageGuard::Factory> d_guardFactory;

    bsl::function<void()> d_onNewAckBatch;
    bsl::function<void(const rmqt::ConsumerAck&)> d_messageGuardCb;
}; // class ConsumerImpl

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_CONSUMERIMPL
