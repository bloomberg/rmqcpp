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

// rmqamqp_receivechannel.cpp   -*-C++-*-
#include <rmqamqp_receivechannel.h>

#include <rmqamqpt_basicconsume.h>
#include <rmqamqpt_basicdeliver.h>
#include <rmqamqpt_basicqos.h>
#include <rmqt_consumerack.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_envelope.h>
#include <rmqt_fieldvalue.h>
#include <rmqt_properties.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bdlt_currenttime.h>
#include <bsls_assert.h>

#include <bsl_algorithm.h>
#include <bsl_map.h>
#include <bsl_optional.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.RECEIVECHANNEL")
using bdlf::PlaceHolders::_1;
using bdlf::PlaceHolders::_2;
using bdlf::PlaceHolders::_3;
} // namespace

class ReceiveChannel::Consumer {
  public:
    enum State { NOT_CONSUMING, STARTING, CONSUMING, CANCELLING, CANCELLED };
    bsl::optional<rmqamqpt::BasicMethod>
    consume(const rmqt::ConsumerConfig& consumerConfig);
    bsl::optional<rmqamqpt::BasicMethod> consumeOk();
    void process(const rmqt::Message& message,
                 const rmqamqpt::BasicDeliver& deliver,
                 size_t lifetimeId);

    /// Signal the server to stop delivering
    bsl::optional<rmqamqpt::BasicMethod> cancel();

    /// Server acknowledged cancel
    void cancelOk();

    ~Consumer();

    Consumer(const bsl::shared_ptr<rmqt::Queue>& queue,
             const MessageCallback& onNewMessage,
             const bsl::string& tag);

    bool isStarted() const;
    bool isActive() const;
    bool shouldRestart() const;

    void reset();

    const bsl::string& consumerTag() const;
    const bsl::string& queueName() const;

  private:
    State d_state;
    bsl::string d_tag;
    bsl::shared_ptr<rmqt::Queue> d_queue;
    MessageCallback d_onNewMessage;
};

ReceiveChannel::Consumer::Consumer(const bsl::shared_ptr<rmqt::Queue>& queue,
                                   const MessageCallback& onNewMessage,
                                   const bsl::string& consumerTag)
: d_state(NOT_CONSUMING)
, d_tag(consumerTag)
, d_queue(queue)
, d_onNewMessage(onNewMessage)
{
}

void ReceiveChannel::Consumer::reset()
{
    if (shouldRestart()) {
        d_state = NOT_CONSUMING;
    }
}

bsl::optional<rmqamqpt::BasicMethod> ReceiveChannel::Consumer::consumeOk()
{
    bsl::optional<rmqamqpt::BasicMethod> method;

    if (d_state == STARTING) {
        d_state = CONSUMING;
    }
    else if (d_state == CANCELLING) {
        // Consumer has been cancelled since sending Basic.Consume
        BALL_LOG_INFO
            << "Received ConsumeOk for a cancelled consumer, cancelling";
        method = rmqamqpt::BasicCancel(d_tag);
    }
    else {
        BALL_LOG_ERROR << "Unexpected ConsumeOK, consumer-tag:  " << d_tag
                       << ", state: " << d_state << ", ignoring";
    }

    return method;
}

const bsl::string& ReceiveChannel::Consumer::consumerTag() const
{
    return d_tag;
}

const bsl::string& ReceiveChannel::Consumer::queueName() const
{
    return d_queue->name();
}

bool ReceiveChannel::Consumer::isStarted() const
{
    return d_state != NOT_CONSUMING;
}
bool ReceiveChannel::Consumer::isActive() const { return d_state == CONSUMING; }
bool ReceiveChannel::Consumer::shouldRestart() const
{
    return d_state < CANCELLING;
}

void ReceiveChannel::Consumer::process(const rmqt::Message& msg,
                                       const rmqamqpt::BasicDeliver& deliver,
                                       size_t lifetimeId)
{
    d_onNewMessage(msg,
                   rmqt::Envelope(deliver.deliveryTag(),
                                  lifetimeId,
                                  deliver.consumerTag(),
                                  deliver.exchange(),
                                  deliver.routingKey(),
                                  deliver.redelivered()));
}

namespace {

rmqt::FieldTable
getBasicConsumeArguments(const rmqt::ConsumerConfig& consumerConfig)
{
    rmqt::FieldTable arguments;

    bsl::optional<int64_t> consumerPriority = consumerConfig.consumerPriority();
    if (consumerPriority) {
        arguments["x-priority"] = rmqt::FieldValue(consumerPriority.value());
    }

    return arguments;
}

} // namespace

bsl::optional<rmqamqpt::BasicMethod>
ReceiveChannel::Consumer::consume(const rmqt::ConsumerConfig& consumerConfig)
{
    bsl::optional<rmqamqpt::BasicMethod> method;

    if (d_state == NOT_CONSUMING) {
        BALL_LOG_INFO << "Starting consumer: " << d_tag
                      << " for queue: " << d_queue->name();
        d_state = STARTING;

        const bool noLocal = false;
        const bool noAck   = false;
        const bool exclusive =
            consumerConfig.exclusiveFlag() == rmqt::Exclusive::ON;
        const bool noWait = false;

        method =
            rmqamqpt::BasicConsume(d_queue->name(),
                                   d_tag,
                                   getBasicConsumeArguments(consumerConfig),
                                   noLocal,
                                   noAck,
                                   exclusive,
                                   noWait);
    }
    else {
        BALL_LOG_ERROR << "Start called in state " << d_state << ", ignoring";
    }

    return method;
}

bsl::optional<rmqamqpt::BasicMethod> ReceiveChannel::Consumer::cancel()
{
    bsl::optional<rmqamqpt::BasicMethod> method;

    if (d_state == CONSUMING) {
        BALL_LOG_INFO << "Cancelling consumer: " << d_tag
                      << " for queue: " << d_queue->name();
        method = rmqamqpt::BasicCancel(d_tag);
    }
    else {
        BALL_LOG_WARN
            << "Cancel called in a state other than CONSUMING. State: "
            << d_state << ", queue: " << queueName();
    }

    d_state = CANCELLING;
    return method;
}

void ReceiveChannel::Consumer::cancelOk()
{
    if (d_state != CANCELLING) {
        BALL_LOG_ERROR << "Received CancelOk without sending Cancel consumer: ["
                       << d_tag << "]";
    }
    d_state = CANCELLED;
}

ReceiveChannel::Consumer::~Consumer() {}

// Constructor
ReceiveChannel::ReceiveChannel(
    const rmqt::Topology& topology,
    const AsyncWriteCallback& messageSender,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
    const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
    const rmqt::ConsumerConfig& consumerConfig,
    const bsl::string& vhost,
    const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
    const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
    const HungChannelCallback& connErrorCb)
: Channel(topology,
          messageSender,
          retryHandler,
          metricPublisher,
          vhost,
          hungProgressTimer,
          connErrorCb)
, d_consumerConfig(consumerConfig)
, d_consumer()
, d_nextMessage()
, d_messageStore()
, d_ackQueue(ackQueue)
, d_multipleAckHandler(
      bdlf::BindUtil::bind(&ReceiveChannel::sendAck, this, _1, _2),
      bdlf::BindUtil::bind(&ReceiveChannel::sendNack, this, _1, _2, _3))
, d_cancelFuturePair()
, d_drainFuture()
{
}

void ReceiveChannel::onOpen()
{
    BALL_LOG_TRACE << "Setting Prefetch: " << d_consumerConfig.prefetchCount();
    d_hungProgressTimer->reset(
        bsls::TimeInterval(Channel::k_HUNG_CHANNEL_TIMER_SEC));
    writeMessage(Message(rmqamqpt::Method(rmqamqpt::BasicMethod(
                     rmqamqpt::BasicQoS(d_consumerConfig.prefetchCount())))),
                 AWAITING_REPLY);
}

void ReceiveChannel::onReset()
{
    d_nextMessage.reset();
    d_multipleAckHandler.reset();

    // Drop and lock all pending acknowledgement batches
    // This reduces the number of 'Ignoring ack/nack for a channel which is
    // closed' alerts by just eliminating the acks.
    typedef bsl::vector<bsl::shared_ptr<rmqt::ConsumerAckBatch> >
        AckBatchVector;

    AckBatchVector clearedAcknowledgements;
    d_ackQueue->removeAll(&clearedAcknowledgements);
    for (AckBatchVector::iterator it = clearedAcknowledgements.begin();
         it != clearedAcknowledgements.end();
         ++it) {
        (*it)->lock();
    }

    // processFailures() clears d_messageStore;

    if (d_consumer) {
        if (d_consumer->shouldRestart()) {
            BALL_LOG_TRACE << "Resetting consumer";
            d_consumer->reset();
        }
        else {
            BALL_LOG_WARN << "Clearing Consumer in Cancelling/Cancelled State";
            d_consumer.reset();
        }
    }
    if (d_cancelFuturePair) {
        d_cancelFuturePair->first(
            rmqt::Result<>("Cancel could not be processed by the server, "
                           "due channel reset, "
                           "however consumer is now in a cancelled state"));
        d_cancelFuturePair.reset();
    }
    if (d_drainFuture) {
        (*d_drainFuture)(
            rmqt::Result<>("ReceiveChannel reset before it was Fully Drained"));
        d_drainFuture.reset();
    }
}

rmqt::Result<> ReceiveChannel::consume(const rmqt::QueueHandle& queue,
                                       const MessageCallback& onNewMessage,
                                       const bsl::string& consumerTag)
{
    bsl::shared_ptr<rmqt::Queue> queuePtr = queue.lock();

    if (!queuePtr) {
        return rmqt::Result<>("QueueHandle expired", 1);
    }

    BSLS_ASSERT(!d_consumer);

    d_consumer =
        bsl::make_shared<Consumer>(queuePtr, onNewMessage, consumerTag);

    if (state() == READY || state() == AWAITING_REPLY) {
        restartConsumers();
    }
    else {
        BALL_LOG_TRACE << "Consume called before channel ready, consumertag: "
                       << consumerTag << " pending";
    }
    // send consume off to server

    return rmqt::Result<>();
}

ReceiveChannel::~ReceiveChannel()
{
    if (d_cancelFuturePair) {
        d_cancelFuturePair->first(
            rmqt::Result<>("ReceiveChannel Shut Down Before CancelOk Received, "
                           "outstanding Acks/Nacks will not be processed"));
        d_cancelFuturePair.reset();
    }
    if (d_drainFuture) {
        (*d_drainFuture)(rmqt::Result<>(
            "ReceiveChannel Shut Down Before Fully Drained (outstanding "
            "messages that should have been acked weren't"));
        d_drainFuture.reset();
    }
}

void ReceiveChannel::consumeAckBatchFromQueue()
{
    if (d_ackQueue->length() == 0) {
        BALL_LOG_ERROR << "Ack queue is empty";
        return;
    }

    bsl::shared_ptr<rmqt::ConsumerAckBatch> batch = d_ackQueue->popFront();
    bsl::vector<rmqt::ConsumerAck>& acks          = batch->lock();

    bsl::vector<rmqt::ConsumerAck>::iterator valid = acks.begin();
    for (bsl::vector<rmqt::ConsumerAck>::iterator it = valid; it < acks.end();
         ++it) {
        if (it->envelope().channelLifetimeId() == lifetimeId()) {
            // Avoid unnecessary copying to the same vector index
            if (it != valid) {
                (*valid) = (*it);
            }
            ++valid;
        }
        else {
            BALL_LOG_WARN << "Ignoring ack/nack for a channel which is "
                             "closed. Delivery tag: "
                          << it->envelope().deliveryTag()
                          << ", message channel lifetime id: "
                          << it->envelope().channelLifetimeId()
                          << ", current channel lifetime id: " << lifetimeId()
                          << ". This happens when the connection drops while a "
                             "message is being processed. The broker will "
                             "safely redeliver the message.";
        }
    }
    acks.erase(valid, acks.end());

    d_multipleAckHandler.process(acks);
}

bool ReceiveChannel::consumerIsActive() const
{
    return d_consumer && d_consumer->isActive();
}

rmqt::Future<> ReceiveChannel::cancel()
{
    if (d_cancelFuturePair) {
        BALL_LOG_WARN << "Cancel called with a cancel already in "
                         "flight";
        return d_cancelFuturePair->second;
    }
    if (d_consumer) {
        if (d_consumer->isStarted()) {
            d_cancelFuturePair =
                bslma::ManagedPtrUtil::makeManaged<rmqt::Future<>::Pair>(
                    rmqt::Future<>::make());
            bsl::optional<rmqamqpt::BasicMethod> method = d_consumer->cancel();
            if (method) {
                writeMessage(Message(rmqamqpt::Method(method.value())),
                             AWAITING_REPLY);
            }
            return d_cancelFuturePair->second;
        }
        // Not consuming so we're already done
        d_consumer.reset();
        return rmqt::Future<>(rmqt::Result<>());
    }
    return rmqt::Future<>(
        rmqt::Result<>("Cancel called, with no active consumer"));
}

rmqt::Future<> ReceiveChannel::drain()
{
    rmqt::Future<>::Pair future = rmqt::Future<>::make();
    if (!d_consumer) {
        if (d_messageStore.count() > 0) {
            d_drainFuture =
                bslma::ManagedPtrUtil::makeManaged<rmqt::Future<>::Maker>(
                    future.first);
        }
        else {
            future.first(rmqt::Result<>());
        }
    }
    else if (!d_drainFuture) {
        future.first(
            rmqt::Result<>("drain() called, but consumer not cancelled"));
    }
    else {
        future.first(rmqt::Result<>("Calling drain() 2x is unsupported"));
    }
    return future.second;
}

MessageStore<rmqt::Message>::MessageList
ReceiveChannel::getMessagesOlderThan(const bdlt::Datetime& cutoffTime) const
{
    return d_messageStore.getMessagesOlderThan(cutoffTime);
}

void ReceiveChannel::processBasicMethod(const rmqamqpt::BasicMethod& basic)
{
    if (!(state() == READY || state() == AWAITING_REPLY)) {
        BALL_LOG_WARN << "Unexpected BasicMethod [" << basic
                      << "] whilst in state: " << state();
    }
    switch (basic.methodId()) {
        case rmqamqpt::BasicDeliver::METHOD_ID: {
            if (d_nextMessage) {
                BALL_LOG_ERROR
                    << "Expecting Content, got another deliver, have: "
                    << *d_nextMessage
                    << ", got: " << basic.the<rmqamqpt::BasicDeliver>();
                close(rmqamqpt::Constants::UNEXPECTED_FRAME,
                      "Expected Content");
            }
            else {
                BALL_LOG_TRACE << "Deliver: "
                               << basic.the<rmqamqpt::BasicDeliver>();
                d_nextMessage =
                    bslma::ManagedPtrUtil::makeManaged<rmqamqpt::BasicDeliver>(
                        basic.the<rmqamqpt::BasicDeliver>());
            }

        } break;
        case rmqamqpt::BasicConsumeOk::METHOD_ID: {
            if (d_consumer &&
                d_consumer->consumerTag() ==
                    basic.the<rmqamqpt::BasicConsumeOk>().consumerTag()) {

                d_hungProgressTimer->cancel();

                bsl::optional<rmqamqpt::BasicMethod> method =
                    d_consumer->consumeOk();
                if (method) {
                    writeMessage(Message(rmqamqpt::Method(method.value())),
                                 AWAITING_REPLY);
                }
            }
            else {
                invalidConsumerError(
                    basic.the<rmqamqpt::BasicConsumeOk>().consumerTag());
            }
            if (state() != READY) {
                if (consumerIsActive()) {
                    ready();
                }
            }
        } break;
        case rmqamqpt::BasicQoSOk::METHOD_ID: {
            if (d_consumer) {
                // we've restarted, we already have a callback so can declare
                // consumer
                restartConsumers();
            }
            else {
                // This is the first connection, we don't have a callback
                // registered so need to wait for the consume() call.
                ready();
            }
        } break;
        case rmqamqpt::BasicCancel::METHOD_ID: {
            if (d_consumer &&
                d_consumer->consumerTag() ==
                    basic.the<rmqamqpt::BasicCancel>().consumerTag()) {
                BALL_LOG_WARN << "Consumer cancelled by broker: "
                              << d_consumer->consumerTag();
                close(rmqamqpt::Constants::REPLY_SUCCESS,
                      "Closing Channel due to consumer cancel");
            }
            else {
                invalidConsumerError(
                    basic.the<rmqamqpt::BasicCancel>().consumerTag());
            }
        } break;
        case rmqamqpt::BasicCancelOk::METHOD_ID: {
            if (d_consumer &&
                d_consumer->consumerTag() ==
                    basic.the<rmqamqpt::BasicCancelOk>().consumerTag()) {
                d_consumer->cancelOk();
                BALL_LOG_INFO << "Stopping Consumer: "
                              << d_consumer->consumerTag();
                d_consumer.reset();
                if (d_cancelFuturePair) {
                    d_cancelFuturePair->first(rmqt::Result<>());
                    d_cancelFuturePair.reset();
                }

                // Ensure any waitForReady() futures are resolved before we
                // enter the fully cancelled state
                ready();
            }
            else {
                invalidConsumerError(
                    basic.the<rmqamqpt::BasicCancelOk>().consumerTag());
            }
        } break;
        default:
            BALL_LOG_ERROR << "Received not implemented basic method: "
                           << basic;
    }
}

void ReceiveChannel::processMessage(const rmqt::Message& message)
{
    if (!d_nextMessage) {
        BALL_LOG_ERROR << "Unexpected Content";
        close(rmqamqpt::Constants::UNEXPECTED_FRAME, "Expected BasicDeliver");
    }
    else {
        if (!d_messageStore.insert(d_nextMessage->deliveryTag(), message)) {
            close(rmqamqpt::Constants::NOT_ALLOWED, "Duplicate DeliveryTag");
        }
        else {
            if (d_consumer &&
                d_consumer->consumerTag() == d_nextMessage->consumerTag()) {

                d_metricPublisher->publishCounter(
                    "received_messages", 1, d_vhostTags);

                d_consumer->process(message, *d_nextMessage, lifetimeId());
                d_nextMessage.reset();
            }
            else {
                close(rmqamqpt::Constants::NOT_FOUND, "Invalid ConsumerTag");
            }
        }
    }
}

void ReceiveChannel::restartConsumers()
{
    if (d_consumer) {
        bsl::optional<rmqamqpt::BasicMethod> method =
            d_consumer->consume(d_consumerConfig);
        if (method) {
            d_hungProgressTimer->reset(
                bsls::TimeInterval(Channel::k_HUNG_CHANNEL_TIMER_SEC));
            writeMessage(Message(rmqamqpt::Method(method.value())),
                         AWAITING_REPLY);
        }
        else {
            BALL_LOG_WARN << "Consumer does not want to be restarted. Queue: "
                          << d_consumer->queueName();
        }
    }
}

void ReceiveChannel::invalidConsumerError(const bsl::string& consumerTag)
{
    BALL_LOG_ERROR << "Received ConsumerTag for unknown Consumer: ["
                   << consumerTag << "]";
    close(rmqamqpt::Constants::NO_CONSUMERS,
          "Bad Consumer Tag: " + consumerTag);
}

void ReceiveChannel::removeSingleMessageFromStore(uint64_t deliveryTag)
{
    rmqt::Message msg;
    bdlt::Datetime insertTime;
    bool success = d_messageStore.remove(deliveryTag, &msg, &insertTime);

    if (!success) {
        BALL_LOG_ERROR << "Tried to remove message not present in the "
                          "message store. Delivery tag: "
                       << deliveryTag;
        return;
    }

    d_metricPublisher->publishDistribution(
        "acknowledge_latency",
        (bdlt::CurrentTime::utc() - insertTime).totalSecondsAsDouble(),
        d_vhostTags);
}

void ReceiveChannel::removeMultipleMessagesFromStore(uint64_t deliveryTag)
{
    MessageStore<rmqt::Message>::MessageList removedMessages =
        d_messageStore.removeUntil(deliveryTag);

    for (MessageStore<rmqt::Message>::MessageList::iterator it =
             removedMessages.begin();
         it != removedMessages.end();
         it++) {
        bdlt::Datetime insertTime = it->second.second;
        d_metricPublisher->publishDistribution(
            "acknowledge_latency",
            (bdlt::CurrentTime::utc() - insertTime).totalSecondsAsDouble(),
            d_vhostTags);
    }
}

void ReceiveChannel::removeMessagesFromStore(uint64_t deliveryTag,
                                             bool multiple)
{
    if (multiple) {
        removeMultipleMessagesFromStore(deliveryTag);
    }
    else {
        removeSingleMessageFromStore(deliveryTag);
    }
    if (!d_messageStore.count() && d_drainFuture) {
        (*d_drainFuture)(rmqt::Result<>());
    }
}

void ReceiveChannel::sendAck(uint64_t deliveryTag, bool multiple)
{
    sendAckOrNack(
        rmqamqpt::BasicMethod(rmqamqpt::BasicAck(deliveryTag, multiple)),
        deliveryTag,
        multiple);
}

void ReceiveChannel::sendNack(uint64_t deliveryTag, bool requeue, bool multiple)
{
    sendAckOrNack(rmqamqpt::BasicMethod(
                      rmqamqpt::BasicNack(deliveryTag, requeue, multiple)),
                  deliveryTag,
                  multiple);
}

void ReceiveChannel::sendAckOrNack(const rmqamqpt::BasicMethod& basicMethod,
                                   uint64_t deliveryTag,
                                   bool multiple)
{
    writeMessage(Message(rmqamqpt::Method(basicMethod)),
                 bdlf::BindUtil::bind(&ReceiveChannel::removeMessagesFromStore,
                                      this,
                                      deliveryTag,
                                      multiple));
}

void ReceiveChannel::processFailures()
{
    const size_t nFailedMsg = d_messageStore.count();
    MessageStore<rmqt::Message> failures;

    // This increments the d_messageStore lifetime which is critical
    // for ensuring stale delivered message ack/nacks are ignored
    d_messageStore.swap(failures);

    if (nFailedMsg > 0) {
        BALL_LOG_INFO
            << nFailedMsg
            << " messages were un-acked at disconnection. These will be "
               "re-delivered by the broker";
    }
}

const char* ReceiveChannel::channelType() const { return "Consumer"; }

bsl::string ReceiveChannel::channelDebugName() const
{
    bsl::string consumerSummary;
    if (d_consumer) {
        consumerSummary = "Queue: " + d_consumer->queueName() +
                          " Consumer Tag: " + d_consumer->consumerTag() + " ";
    }
    else {
        consumerSummary = "No Consumer ";
    }

    return "Consumer Channel: " + consumerSummary + bsl::to_string(inFlight()) +
           " in-flight messages";
}

} // namespace rmqamqp
} // namespace BloombergLP
