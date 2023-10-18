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

#ifndef INCLUDED_RMQTESTUTIL_MOCKCHANNEL_T
#define INCLUDED_RMQTESTUTIL_MOCKCHANNEL_T

#include <bsl_memory.h>
#include <rmqamqp_channel.h>
#include <rmqamqp_messagestore.h>
#include <rmqamqp_receivechannel.h>
#include <rmqamqp_sendchannel.h>
#include <rmqio_asioeventloop.h>
#include <rmqio_retryhandler.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_result.h>
#include <rmqt_topology.h>
#include <rmqtestutil_mockmetricpublisher.h>
#include <rmqtestutil_mockretryhandler.t.h>
#include <rmqtestutil_mocktimerfactory.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace BloombergLP {
namespace rmqtestutil {

inline void
noopWriteCallback(const bsl::shared_ptr<rmqamqp::Message>&,
                  const rmqio::Connection::SuccessWriteCallback& callback)
{
    callback();
}

inline void noopHungCallback() {}
inline void noopHungTimerCallback(rmqio::Timer::InterruptReason) {}

class MockChannel : public rmqamqp::Channel {
  public:
    explicit MockChannel(
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler =
            bsl::make_shared<MockRetryHandler>(),
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& timerFactory =
            bsl::make_shared<rmqtestutil::MockTimerFactory>())
    : Channel(rmqt::Topology(),
              &noopWriteCallback,
              retryHandler,
              bsl::make_shared<MockMetricPublisher>(),
              "blankvhost",
              timerFactory->createWithCallback(&noopHungTimerCallback),
              &noopHungCallback)
    , d_timerFactory(timerFactory)
    {
    }

    MOCK_METHOD0(open, void());
    MOCK_METHOD1(reset, Channel::CleanupIndicator(bool));
    MOCK_METHOD4(close,
                 void(rmqamqpt::Constants::AMQPReplyCode,
                      bsl::string,
                      rmqamqpt::Constants::AMQPClassId,
                      rmqamqpt::Constants::AMQPMethodId));
    MOCK_METHOD1(processReceived,
                 Channel::CleanupIndicator(const rmqamqp::Message& message));
    MOCK_METHOD1(processBasicMethod, void(const rmqamqpt::BasicMethod& basic));
    MOCK_METHOD1(processConfirmMethod,
                 void(const rmqamqpt::ConfirmMethod& confirm));
    MOCK_METHOD0(processFailures, void());
    MOCK_CONST_METHOD0(inFlight, size_t());
    MOCK_CONST_METHOD0(lifetimeId, size_t());
    MOCK_CONST_METHOD0(channelType, const char*());

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
};
class MockReceiveChannel : public rmqamqp::ReceiveChannel {
  public:
    MockReceiveChannel(
        const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler =
            bsl::make_shared<MockRetryHandler>(),
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& timerFactory =
            bsl::make_shared<rmqtestutil::MockTimerFactory>(),
        const rmqt::ConsumerConfig& consumerConfig =
            rmqt::ConsumerConfig(rmqt::ConsumerConfig::generateConsumerTag(),
                                 100))
    : rmqamqp::ReceiveChannel(
          rmqt::Topology(),
          &noopWriteCallback,
          retryHandler,
          bsl::make_shared<MockMetricPublisher>(),
          consumerConfig,
          "blankvhost",
          ackQueue,
          timerFactory->createWithCallback(&noopHungTimerCallback),
          &noopHungCallback)
    , d_timerFactory(timerFactory)
    {
        ON_CALL(*this, consume(testing::_, testing::_, testing::_))
            .WillByDefault(testing::Return(rmqt::Result<>()));
    }

    MOCK_METHOD0(waitForReady, rmqt::Future<>());
    MOCK_METHOD0(gracefulClose, void());
    MOCK_METHOD0(open, void());
    MOCK_METHOD1(reset, Channel::CleanupIndicator(bool));
    MOCK_METHOD4(close,
                 void(rmqamqpt::Constants::AMQPReplyCode,
                      bsl::string,
                      rmqamqpt::Constants::AMQPClassId,
                      rmqamqpt::Constants::AMQPMethodId));
    MOCK_METHOD1(processReceived,
                 Channel::CleanupIndicator(const rmqamqp::Message& message));
    MOCK_METHOD3(consume,
                 rmqt::Result<>(const rmqt::QueueHandle&,
                                const MessageCallback& onNewMessage,
                                const bsl::string&));
    MOCK_METHOD0(consumeAckBatchFromQueue, void());
    MOCK_METHOD0(cancel, rmqt::Future<>());
    MOCK_METHOD0(drain, rmqt::Future<>());
    MOCK_CONST_METHOD1(getMessagesOlderThan,
                       rmqamqp::MessageStore<rmqt::Message>::MessageList(
                           const bdlt::Datetime&));

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
};

class MockUpdateReceiveChannel : public rmqamqp::ReceiveChannel {
  public:
    MockUpdateReceiveChannel(
        const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler =
            bsl::make_shared<MockRetryHandler>(),
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& timerFactory =
            bsl::make_shared<rmqtestutil::MockTimerFactory>(),
        const rmqt::ConsumerConfig& consumerConfig =
            rmqt::ConsumerConfig(rmqt::ConsumerConfig::generateConsumerTag(),
                                 100))
    : rmqamqp::ReceiveChannel(
          rmqt::Topology(),
          noopWriteCallback,
          retryHandler,
          bsl::make_shared<MockMetricPublisher>(),
          consumerConfig,
          "blankvhost",
          ackQueue,
          timerFactory->createWithCallback(&noopHungTimerCallback),
          &noopHungCallback)
    , d_timerFactory(timerFactory)
    {
        ready();
    }

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
};

class MockSendChannel : public rmqamqp::SendChannel {
  public:
    explicit MockSendChannel(
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler =
            bsl::make_shared<MockRetryHandler>(),
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& timerFactory =
            bsl::make_shared<rmqtestutil::MockTimerFactory>())
    : rmqamqp::SendChannel(
          rmqt::Topology(),
          bsl::make_shared<rmqt::Exchange>("exchange"),
          noopWriteCallback,
          retryHandler,
          bsl::make_shared<MockMetricPublisher>(),
          "blankvhost",
          timerFactory->createWithCallback(&noopHungTimerCallback),
          &noopHungCallback)
    , d_timerFactory(timerFactory)
    {
    }

    MOCK_METHOD0(waitForReady, rmqt::Future<>());
    MOCK_METHOD0(open, void());
    MOCK_METHOD1(reset, Channel::CleanupIndicator(bool));
    MOCK_METHOD4(close,
                 void(rmqamqpt::Constants::AMQPReplyCode,
                      bsl::string,
                      rmqamqpt::Constants::AMQPClassId,
                      rmqamqpt::Constants::AMQPMethodId));
    MOCK_METHOD1(processReceived,
                 Channel::CleanupIndicator(const rmqamqp::Message& message));
    MOCK_METHOD3(publishMessage,
                 void(const rmqt::Message&,
                      const bsl::string&,
                      rmqt::Mandatory::Value));
    MOCK_METHOD1(setCallback, void(const MessageConfirmCallback&));

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
};

class MockUpdateSendChannel : public rmqamqp::SendChannel {
  public:
    explicit MockUpdateSendChannel(
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler =
            bsl::make_shared<MockRetryHandler>(),
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& timerFactory =
            bsl::make_shared<rmqtestutil::MockTimerFactory>())
    : rmqamqp::SendChannel(
          rmqt::Topology(),
          bsl::make_shared<rmqt::Exchange>("exchange"),
          noopWriteCallback,
          retryHandler,
          bsl::make_shared<MockMetricPublisher>(),
          "blankvhost",
          timerFactory->createWithCallback(&noopHungTimerCallback),
          &noopHungCallback)
    , d_timerFactory(timerFactory)
    {
        ready();
    }

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif
