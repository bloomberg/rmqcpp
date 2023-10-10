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

// rmqamqp_sendchannel.h
#ifndef INCLUDED_RMQAMQP_SENDCHANNEL_H
#define INCLUDED_RMQAMQP_SENDCHANNEL_H

#include <rmqamqp_channel.h>
#include <rmqamqp_messagestore.h>
#include <rmqamqp_messagewithroute.h>
#include <rmqamqpt_basicreturn.h>
#include <rmqt_confirmresponse.h>
#include <rmqt_message.h>

#include <bsl_functional.h>
#include <bsl_map.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_queue.h>
#include <bsl_string.h>
#include <bslma_managedptr.h>

namespace BloombergLP {
namespace rmqamqp {

class SendChannel : public Channel {
  public:
    typedef bsl::function<void(const rmqt::Message&,
                               const bsl::string& routingKey,
                               const rmqt::ConfirmResponse& confirmResponse)>
        MessageConfirmCallback;

    SendChannel(const rmqt::Topology& topology,
                const bsl::shared_ptr<rmqt::Exchange>& exchange,
                const Channel::AsyncWriteCallback& onAsyncWrite,
                const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
                const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
                const bsl::string& vhost,
                const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
                const HungChannelCallback& connErrorCb);

    virtual ~SendChannel();

    /// Publish a message to broker for the exchange passed at
    /// construction, with the given routingKey & mandatory flag
    virtual void publishMessage(const rmqt::Message& message,
                                const bsl::string& routingKey,
                                rmqt::Mandatory::Value mandatory);

    /// Set the confirmation callback function
    /// Must be called before the first call to `publishMessage`
    virtual void setCallback(const MessageConfirmCallback& onMessageConfirm);

    size_t inFlight() const BSLS_KEYWORD_OVERRIDE
    {
        return d_messageStore.count();
    }

    size_t lifetimeId() const BSLS_KEYWORD_OVERRIDE
    {
        return d_messageStore.lifetimeId();
    }

    bsl::string channelDebugName() const BSLS_KEYWORD_OVERRIDE;

  private:
    void processBasicMethod(const rmqamqpt::BasicMethod& basic)
        BSLS_KEYWORD_OVERRIDE;
    void processConfirmMethod(const rmqamqpt::ConfirmMethod& confirm)
        BSLS_KEYWORD_OVERRIDE;
    void processMessage(const rmqt::Message& message) BSLS_KEYWORD_OVERRIDE;

    void onReset() BSLS_KEYWORD_OVERRIDE;
    void onFlowAllowed() BSLS_KEYWORD_OVERRIDE;
    void processFailures() BSLS_KEYWORD_OVERRIDE;
    void onOpen() BSLS_KEYWORD_OVERRIDE;

    // Send Confirm.Select method to turn on confirm delivery

    // Publish all messages from pendingMessages queue.
    // Should be called immediately after re-opening channel
    void publishPendingMessages();

    void readyToPublishMsg(const rmqt::Message& message,
                           const bsl::string& routingKey,
                           const rmqt::Mandatory::Value mandatory);

    void processAckNack(bool multiple,
                        size_t deliveryTag,
                        const rmqt::ConfirmResponse& confirmResponse);

    void
    callbackMessages(const MessageStore<MessageWithRoute>::MessageList& confs,
                     const rmqt::ConfirmResponse& confirmResponse);

    const char* channelType() const BSLS_KEYWORD_OVERRIDE;

  private:
    SendChannel(const SendChannel& copy) BSLS_KEYWORD_DELETED;
    SendChannel& operator=(const SendChannel&) BSLS_KEYWORD_DELETED;

    class BasicMethodProcessor;

    rmqamqp::MessageStore<MessageWithRoute> d_messageStore;
    MessageConfirmCallback d_confirmCallback;

    /// Stores messages until channel is ready to send them
    bsl::queue<MessageWithRoute> d_pendingMessages;
    bsl::shared_ptr<rmqt::Exchange> d_exchange;

    uint64_t d_deliveryCounter;

    bslma::ManagedPtr<rmqamqpt::BasicReturn> d_basicReturn;
    bsl::map<uint64_t, rmqt::ConfirmResponse> d_returnedTagResponse;
}; // class SendChannel

bsl::ostream& operator<<(bsl::ostream&, rmqt::ConfirmResponse::Status);

} // namespace rmqamqp
} // namespace BloombergLP
#endif
