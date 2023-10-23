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

#include <rmqamqp_sendchannel.h>

#include <rmqamqpt_basicpublish.h>
#include <rmqamqpt_constants.h>
#include <rmqt_exchange.h>

#include <bdlt_currenttime.h>
#include <bsls_assert.h>

#include <bsl_memory.h>
#include <bsl_numeric.h>
#include <bsl_ostream.h>
#include <bsl_utility.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.SENDCHANNEL")

void noopWriteHandler() {}

void printPartialReturns(
    const bsl::map<uint64_t, rmqt::ConfirmResponse>& returnNoAck,
    const MessageStore<MessageWithRoute>& store)
{
    if (!returnNoAck.empty()) {
        bsl::stringstream st;

        for (bsl::map<uint64_t, rmqt::ConfirmResponse>::const_iterator it =
                 returnNoAck.begin();
             it != returnNoAck.end();
             ++it) {

            MessageWithRoute partialReturn;
            store.lookup(it->first, &partialReturn);

            if (it != returnNoAck.begin()) {
                // Comma at the beginning to separate from the previous
                // return. The very first return doesn't have previous,
                // so comma is skipped in this case.
                st << ",";
            }
            st << "[" << it->first << ":'"
               << partialReturn.message().messageId() << "':" << it->second
               << "]";
        }

        BALL_LOG_WARN << "Messages with the following "
                         "[deliveryTag:'message-id':code:reason] were "
                         "partially returned: ["
                      << st.str() << "]";
    }
}

} // namespace

SendChannel::SendChannel(
    const rmqt::Topology& topology,
    const bsl::shared_ptr<rmqt::Exchange>& exchange,
    const AsyncWriteCallback& messageSender,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
    const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
    const bsl::string& vhost,
    const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
    const HungChannelCallback& connErrorCb)
: Channel(topology,
          messageSender,
          retryHandler,
          metricPublisher,
          vhost,
          hungProgressTimer,
          connErrorCb)
, d_messageStore()
, d_confirmCallback()
, d_pendingMessages()
, d_exchange(exchange)
, d_deliveryCounter(1)
, d_basicReturn()
, d_returnedTagResponse()
{
}

SendChannel::~SendChannel() {}

void SendChannel::setCallback(const MessageConfirmCallback& onMessageConfirm)
{
    d_confirmCallback = onMessageConfirm;
}

void SendChannel::onReset()
{
    BALL_LOG_DEBUG << "Channel Reset New LifetimeId: "
                   << d_messageStore.lifetimeId();
    d_deliveryCounter = 1;
    d_basicReturn.reset();
}

void SendChannel::onFlowAllowed()
{
    if (state() == READY) {
        publishPendingMessages();
    }
}

void SendChannel::processFailures()
{
    MessageStore<MessageWithRoute> store;
    d_messageStore.swap(store);
    const size_t count = store.count();
    while (store.count() > 0) {
        MessageWithRoute msg;
        bdlt::Datetime unused;
        store.remove(store.oldestTagInStore(), &msg, &unused);
        d_pendingMessages.push(msg);
    }

    printPartialReturns(d_returnedTagResponse, store);
    d_returnedTagResponse.clear();

    if (count) {
        BALL_LOG_INFO
            << "Queuing " << count
            << " outstanding messages to be resent after reconnection";
    }
}

void SendChannel::onOpen()
{
    BALL_LOG_TRACE << "Turning on confirm delivery for the channel";
    writeMessage(Message(rmqamqpt::Method(
                     rmqamqpt::ConfirmMethod(rmqamqpt::ConfirmSelect(false)))),
                 AWAITING_REPLY);
}

void SendChannel::publishMessage(const rmqt::Message& message,
                                 const bsl::string& routingKey,
                                 rmqt::Mandatory::Value mandatory)
{
    BSLS_ASSERT(d_confirmCallback);

    d_metricPublisher->publishCounter("client_sent_messages", 1, d_vhostTags);

    if (state() != READY || !d_flow) {
        d_pendingMessages.push(
            MessageWithRoute(message, routingKey, mandatory));
        BALL_LOG_INFO << "Channel not ready. Message queued as pending. "
                      << message << " " << d_pendingMessages.size()
                      << " messages pending.";
        return;
    }

    readyToPublishMsg(message, routingKey, mandatory);
}

void SendChannel::readyToPublishMsg(const rmqt::Message& message,
                                    const bsl::string& routingKey,
                                    const rmqt::Mandatory::Value mandatory)
{
    if (d_messageStore.insert(
            d_deliveryCounter,
            MessageWithRoute(message, routingKey, mandatory))) {
        ++d_deliveryCounter; // only increment delivery counter if we
                             // successfully add to msg store
    }

    const bool mandatoryFlag = mandatory == rmqt::Mandatory::RETURN_UNROUTABLE;

    // Not implemented (connection is closed if set) in rabbitmq 3.0+
    const bool immediateFlag = false;

    writeMessage(
        Message(rmqamqpt::Method(rmqamqpt::BasicMethod(rmqamqpt::BasicPublish(
            d_exchange->name(), routingKey, mandatoryFlag, immediateFlag)))),
        &noopWriteHandler);

    d_metricPublisher->publishCounter("published_messages", 1, d_vhostTags);
    writeMessage(Message(message), &noopWriteHandler);
}

void SendChannel::publishPendingMessages()
{
    BALL_LOG_INFO << "Publishing " << d_pendingMessages.size()
                  << " pending messages.";
    while (!d_pendingMessages.empty()) {
        const MessageWithRoute& msg = d_pendingMessages.front();
        readyToPublishMsg(msg.message(), msg.routingKey(), msg.mandatory());
        d_pendingMessages.pop();
    }
}

void SendChannel::processConfirmMethod(const rmqamqpt::ConfirmMethod& confirm)
{
    if (!(state() == AWAITING_REPLY)) {
        BALL_LOG_WARN << "Unexpected confirmMethod [" << confirm
                      << "] whilst in state: " << state();
    }
    switch (confirm.methodId()) {
        case rmqamqpt::ConfirmSelectOk::METHOD_ID: {
            ready();

            BALL_LOG_INFO << "Producer for exchange '" << d_exchange->name()
                          << "' is now ready";

            publishPendingMessages();
        } break;
        default:
            BALL_LOG_ERROR << "Received not implemented confirm method: "
                           << confirm;
    }
}

void SendChannel::processMessage(const rmqt::Message& message)
{
    if (!d_basicReturn) {
        BALL_LOG_ERROR << "Unexpected Content";
        close(rmqamqpt::Constants::UNEXPECTED_FRAME, "Expected BasicReturn");
    }
    else {
        MessageWithRoute storedMsg;
        uint64_t deliveryTag;
        if (!d_messageStore.lookup(message.guid(), &storedMsg, &deliveryTag)) {
            close(rmqamqpt::Constants::NOT_ALLOWED, "Unknown returned message");
        }
        else {
            d_returnedTagResponse.insert(bsl::make_pair(
                deliveryTag,
                rmqt::ConfirmResponse(d_basicReturn->replyCode(),
                                      d_basicReturn->replyText())));

            BALL_LOG_WARN
                << "Published message was returned by the broker. "
                   "It will not be queued or retried. Returned message: "
                << message << "Method basic.return: " << *d_basicReturn;

            if (message != storedMsg.message()) {
                BALL_LOG_ERROR
                    << "Stored Message: " << storedMsg.message()
                    << " is not equal to returned message: " << message
                    << " This is an issue in this library or RabbitMQ.";
            }
            d_basicReturn.reset();
        }
    }
}

void SendChannel::callbackMessages(
    const MessageStore<MessageWithRoute>::MessageList& confs,
    const rmqt::ConfirmResponse& confirmResponse)
{
    for (MessageStore<MessageWithRoute>::MessageList::const_iterator it =
             confs.cbegin();
         it != confs.cend();
         ++it) {

        bsl::map<uint64_t, rmqt::ConfirmResponse>::const_iterator
            returnedDeliveryTag = d_returnedTagResponse.find(it->first);

        const rmqt::ConfirmResponse& actualResponse =
            (returnedDeliveryTag != d_returnedTagResponse.end())
                ? returnedDeliveryTag->second
                : confirmResponse;

        const MessageWithRoute& msg = it->second.first;
        BALL_LOG_TRACE << actualResponse << " for " << msg;
        d_confirmCallback(msg.message(), msg.routingKey(), actualResponse);

        d_metricPublisher->publishDistribution(
            "confirm_latency",
            (bdlt::CurrentTime::utc() - it->second.second)
                .totalSecondsAsDouble(),
            d_vhostTags);

        if (returnedDeliveryTag != d_returnedTagResponse.end()) {
            d_returnedTagResponse.erase(returnedDeliveryTag);
        }
    }
}

void SendChannel::processAckNack(bool multiple,
                                 size_t deliveryTag,
                                 const rmqt::ConfirmResponse& confirmResponse)
{
    bool success = false;
    MessageStore<MessageWithRoute>::MessageList msgs;
    if (multiple) {
        msgs    = d_messageStore.removeUntil(deliveryTag);
        success = msgs.size();
    }
    else {
        bsl::pair<MessageWithRoute, bdlt::Datetime> msgTime;

        success =
            d_messageStore.remove(deliveryTag, &msgTime.first, &msgTime.second);

        msgs.push_back(bsl::make_pair(deliveryTag, msgTime));
    }
    if (success) {
        callbackMessages(msgs, confirmResponse);
    }
    else {
        BALL_LOG_WARN << "Failed to find message(s) with delivery "
                      << bsl::string(multiple ? "tags up to: " : "tag: ")
                      << deliveryTag
                      << ", latest tag: " << d_messageStore.latestTagInStore();
    }
}

class SendChannel::BasicMethodProcessor {
    SendChannel& d_sendChannel;

  public:
    typedef bool ReturnType;

    BasicMethodProcessor(SendChannel& sendChannel)
    : d_sendChannel(sendChannel)
    {
    }

    ReturnType operator()(const rmqamqpt::BasicNack& nack)
    {
        d_sendChannel.processAckNack(
            nack.multiple(),
            nack.deliveryTag(),
            rmqt::ConfirmResponse(rmqt::ConfirmResponse::REJECT));
        return true;
    }

    ReturnType operator()(const rmqamqpt::BasicAck& ack)
    {
        d_sendChannel.processAckNack(
            ack.multiple(),
            ack.deliveryTag(),
            rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK));
        return true;
    }

    ReturnType operator()(const rmqamqpt::BasicReturn& basicReturn)
    {
        // This method returns an undeliverable message that was published with
        // the "immediate" flag set, or an unroutable message published with the
        // "mandatory" flag set. The reply code and text provide information
        // about the reason that the message was undeliverable.

        if (d_sendChannel.d_basicReturn) {
            BALL_LOG_ERROR
                << "Expecting Basic.Ack, got another Basic.Return, have: "
                << *(d_sendChannel.d_basicReturn) << ", got: " << basicReturn;
            d_sendChannel.close(rmqamqpt::Constants::UNEXPECTED_FRAME,
                                "Expected Content");
            return false;
        }
        d_sendChannel.d_basicReturn =
            bslma::ManagedPtrUtil::makeManaged<rmqamqpt::BasicReturn>(
                basicReturn);
        return true;
    }

    ReturnType operator()(const bslmf::Nil&) { return false; }

    template <typename T>
    ReturnType operator()(const T& t)
    {
        BALL_LOG_ERROR << "Unsupported BasicMethod: " << t;
        return true;
    }
};

void SendChannel::processBasicMethod(const rmqamqpt::BasicMethod& basic)
{
    BasicMethodProcessor bmp(*this);
    basic.apply(bmp);
}
const char* SendChannel::channelType() const { return "Producer"; }

bsl::string SendChannel::channelDebugName() const
{
    return "Producer Channel: Exchange: " + d_exchange->name() +
           ". In-flight Messages: " + bsl::to_string(d_messageStore.count()) +
           ". Pending Messages: " + bsl::to_string(d_pendingMessages.size());
}

bsl::ostream& operator<<(bsl::ostream& os, rmqt::ConfirmResponse::Status status)
{
    switch (status) {
        case rmqt::ConfirmResponse::ACK:
            return os << "confirm";
        case rmqt::ConfirmResponse::RETURN:
            return os << "return";
        case rmqt::ConfirmResponse::REJECT:
            return os << "reject";
    }
    return os << "Invalid status code";
}

} // namespace rmqamqp
} // namespace BloombergLP
