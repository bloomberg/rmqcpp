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

// rmqamqp_receivechannel.h
#ifndef INCLUDED_RMQAMQP_RECEIVECHANNEL_H
#define INCLUDED_RMQAMQP_RECEIVECHANNEL_H

//@PURPOSE: Receive (from server direction) AMQP Channel

//@CLASSES: ReceiveChannel
//
//@AUTHOR: whoy1
//
/// Usage Examples
///--------------
// TBD
//..
//..

// Includes
#include <rmqamqp_channel.h>
#include <rmqamqp_message.h>
#include <rmqamqp_messagestore.h>
#include <rmqamqp_multipleackhandler.h>
#include <rmqio_serializedframe.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_envelope.h>
#include <rmqt_future.h>
#include <rmqt_queue.h>
#include <rmqt_result.h>
#include <rmqt_topology.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_unordered_map.h>
#include <bsls_keyword.h>

namespace BloombergLP {
namespace rmqamqp {

//=======
// Class ReceiveChannel
//=======

class ReceiveChannel : public Channel {
  public:
    typedef bsl::function<void(const rmqt::Message&, const rmqt::Envelope&)>
        MessageCallback;

    // Constructor
    /// @param topology
    /// @param frameSender to facilitate sending frames
    /// @param retryHandler
    /// @param metricPublisher
    /// @param consumerConfig includes prefetch count etc
    /// @param vhost virtual host to set the receive channel on
    /// @param ackQueue
    ReceiveChannel(
        const rmqt::Topology& topology,
        const Channel::AsyncWriteCallback& frameSender,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
        const rmqt::ConsumerConfig& consumerConfig,
        const bsl::string& vhost,
        const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
        const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
        const HungChannelCallback& connErrorCb);

    /// validates the queue handle references a queue in the channel topology,
    // starts the consumer (basic.consume) on that queue, with consumer tag
    // passed in, to the MessageCallback, sets the channel QoS to prefetch value
    virtual rmqt::Result<> consume(const rmqt::QueueHandle&,
                                   const MessageCallback& onNewMessage,
                                   const bsl::string& consumerTag = "");

    virtual ~ReceiveChannel() BSLS_KEYWORD_OVERRIDE;

    virtual void consumeAckBatchFromQueue();

    virtual bool consumerIsActive() const;

    /// Cancels the active consumer on the channel
    /// returns Future that will resolve when CancelOk received from server
    virtual rmqt::Future<> cancel();

    /// If the channel is in a cancelled state, waits for number of the
    /// messages in the message store to reach 0 before resolving the future,
    /// if the channel is not in a cancelled state then the Future will resolve
    /// immediately with an error result
    virtual rmqt::Future<> drain();

    size_t inFlight() const BSLS_KEYWORD_OVERRIDE
    {
        return d_messageStore.count();
    }

    size_t lifetimeId() const BSLS_KEYWORD_OVERRIDE
    {
        return d_messageStore.lifetimeId();
    }

    bsl::string channelDebugName() const BSLS_KEYWORD_OVERRIDE;

    virtual MessageStore<rmqt::Message>::MessageList
    getMessagesOlderThan(const bdlt::Datetime& cutoffTime) const;

  protected:
    void onOpen() BSLS_KEYWORD_OVERRIDE;

  private:
    class Consumer;
    void processBasicMethod(const rmqamqpt::BasicMethod& basic)
        BSLS_KEYWORD_OVERRIDE;
    void processMessage(const rmqt::Message& message) BSLS_KEYWORD_OVERRIDE;

    void onReset() BSLS_KEYWORD_OVERRIDE;
    void processFailures() BSLS_KEYWORD_OVERRIDE;
    void restartConsumers();
    void invalidConsumerError(const bsl::string& consumerTag);
    void removeSingleMessageFromStore(uint64_t deliveryTag);
    void removeMultipleMessagesFromStore(uint64_t deliveryTag);
    void removeMessagesFromStore(uint64_t deliveryTag, bool multiple);
    void sendAck(uint64_t deliveryTag, bool multiple = false);
    void
    sendNack(uint64_t deliveryTag, bool requeue = true, bool multiple = false);
    void sendAckOrNack(const rmqamqpt::BasicMethod& basicMethod,
                       uint64_t deliveryTag,
                       bool multiple);

    const char* channelType() const BSLS_KEYWORD_OVERRIDE;

  private:
    ReceiveChannel(const ReceiveChannel& copy) BSLS_KEYWORD_DELETED;
    ReceiveChannel& operator=(const ReceiveChannel&) BSLS_KEYWORD_DELETED;

    rmqt::ConsumerConfig d_consumerConfig;
    bsl::shared_ptr<Consumer> d_consumer;
    bslma::ManagedPtr<rmqamqpt::BasicDeliver> d_nextMessage;
    rmqamqp::MessageStore<rmqt::Message> d_messageStore;
    bsl::shared_ptr<rmqt::ConsumerAckQueue> d_ackQueue;
    MultipleAckHandler d_multipleAckHandler;
    bslma::ManagedPtr<rmqt::Future<>::Pair> d_cancelFuturePair;
    bslma::ManagedPtr<rmqt::Future<>::Maker> d_drainFuture;
};

} // namespace rmqamqp
} // namespace BloombergLP
#endif
