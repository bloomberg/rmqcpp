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

#include <rmqamqp_channeltests.t.h>
#include <rmqamqp_framer.h>
#include <rmqamqp_metrics.h>
#include <rmqamqpt_method.h>
#include <rmqio_retryhandler.h>
#include <rmqt_confirmresponse.h>
#include <rmqt_exchange.h>
#include <rmqt_message.h>
#include <rmqtestutil_mockmetricpublisher.h>
#include <rmqtestutil_mockretryhandler.t.h>

#include <bdlf_bind.h>
#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

#include <bsl_utility.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

namespace {

const char* TEST_VHOST = "vhostname";

void noopHungCallback() {}

void noopHungTimerCallback(rmqio::Timer::InterruptReason) {}

class MockConfirm {
  public:
    MOCK_METHOD3(conf,
                 void(const rmqt::Message&,
                      const bsl::string&, // routing_key
                      const rmqt::ConfirmResponse&));
};

class SendChannelTestsBase : public rmqamqp::ChannelTests {
  public:
    bsl::shared_ptr<rmqt::Exchange> d_exchange;
    bsl::string d_routingKey;
    MockConfirm d_mockConfirm;
    rmqamqp::SendChannel::MessageConfirmCallback d_onConfirm;
    bsl::shared_ptr<rmqtestutil::MockMetricPublisher> d_metricPublisher;
    bsl::vector<bsl::pair<bsl::string, bsl::string> > d_vhostTag;

    SendChannelTestsBase(
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& timerFactory =
            bsl::make_shared<rmqtestutil::MockTimerFactory>())
    : rmqamqp::ChannelTests()
    , d_exchange(bsl::make_shared<rmqt::Exchange>("test-exchange"))
    , d_routingKey("test-routing-key")
    , d_mockConfirm()
    , d_onConfirm(bdlf::BindUtil::bind(&MockConfirm::conf,
                                       &d_mockConfirm,
                                       bdlf::PlaceHolders::_1,
                                       bdlf::PlaceHolders::_2,
                                       bdlf::PlaceHolders::_3))
    , d_metricPublisher(bsl::make_shared<rmqtestutil::MockMetricPublisher>())
    , d_timerFactory(timerFactory)
    {
        d_vhostTag.push_back(bsl::pair<bsl::string, bsl::string>(
            rmqamqp::Metrics::VHOST_TAG, TEST_VHOST));
    }

    void selectExpectations()
    {
        EXPECT_CALL(
            d_callback,
            onAsyncWrite(
                Pointee(rmqamqp::MessageEq(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::ConfirmMethod(rmqamqpt::ConfirmSelect(false)))))),
                _))
            .WillOnce(InvokeArgument<1>()); // select
    }

    void selectOkReply(rmqamqp::SendChannel& sc)
    {
        sc.processReceived(rmqamqp::Message(rmqamqpt::Method(
            rmqamqpt::ConfirmMethod(rmqamqpt::ConfirmSelectOk()))));
    }

    void startupExpectations(rmqamqp::SendChannel& sc)
    {
        openAndSendTopology(sc);

        selectExpectations();
        queueDeclareReply(sc);

        EXPECT_THAT(sc.state(), Eq(rmqamqp::Channel::AWAITING_REPLY));
        selectOkReply(sc);
        EXPECT_THAT(sc.state(), Eq(rmqamqp::Channel::READY));
    }

    void publishMessage(rmqamqp::SendChannel& sc, const rmqt::Message& msg)
    {
        expectMessages(msg, 1);
        sc.publishMessage(
            msg, d_routingKey, rmqt::Mandatory::RETURN_UNROUTABLE);
    }

    void publishMessages(rmqamqp::SendChannel& sc, size_t count)
    {
        for (size_t i = 0; i < count; ++i) {
            publishMessage(sc, rmqt::Message());
        }
    }

    void receiveAck(rmqamqp::SendChannel& sc,
                    size_t deliveryTag,
                    bool multiple = false)
    {
        sc.processReceived(rmqamqp::Message(rmqamqpt::Method(
            rmqamqpt::BasicMethod(rmqamqpt::BasicAck(deliveryTag, multiple)))));
    }
    void expectMessageNotPublished(rmqt::Message message)
    {
        EXPECT_CALL(
            d_callback,
            onAsyncWrite(
                Pointee(rmqamqp::MessageEq(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::BasicMethod(rmqamqpt::BasicPublish(
                        d_exchange->name(), d_routingKey, true, false)))))),
                _))
            .Times(0);

        EXPECT_CALL(
            d_callback,
            onAsyncWrite(Pointee(rmqamqp::MessageEq(rmqamqp::Message(message))),
                         _))
            .Times(0);
    }

    void
    expectMessages(rmqt::Message message, size_t count, bool mandatory = true)
    {
        EXPECT_CALL(
            d_callback,
            onAsyncWrite(Pointee(rmqamqp::MessageEq(rmqamqp::Message(
                             rmqamqpt::Method(rmqamqpt::BasicMethod(
                                 rmqamqpt::BasicPublish(d_exchange->name(),
                                                        d_routingKey,
                                                        mandatory,
                                                        false)))))),
                         _))
            .Times(count)
            .WillRepeatedly(InvokeArgument<1>());

        EXPECT_CALL(
            d_callback,
            onAsyncWrite(Pointee(rmqamqp::MessageEq(rmqamqp::Message(message))),
                         _))
            .Times(count)
            .WillRepeatedly(InvokeArgument<1>());
    }

    void expectFlowOkMessage(bool active)
    {
        EXPECT_CALL(d_callback,
                    onAsyncWrite(Pointee(rmqamqp::MessageEq(rmqamqp::Message(
                                     rmqamqpt::Method(rmqamqpt::ChannelMethod(
                                         rmqamqpt::ChannelFlowOk(active)))))),
                                 _))
            .Times(1);
    }

    void expectAnyCounterMetric(const bsl::string& name)
    {
        EXPECT_CALL(*d_metricPublisher, publishCounter(name, _, _));
    }

    void
    expectCounterMetric(const bsl::string& name, double value, int times = 1)
    {
        EXPECT_CALL(*d_metricPublisher, publishCounter(name, value, d_vhostTag))
            .Times(times);
    }

    void expectNoCounterMetric(const bsl::string& name)
    {
        EXPECT_CALL(*d_metricPublisher, publishCounter(name, _, _)).Times(0);
    }

    void expectAnyDistributionMetric(const bsl::string& name)
    {
        EXPECT_CALL(*d_metricPublisher, publishDistribution(name, _, _));
    }

    void receiveNack(rmqamqp::SendChannel& sc,
                     size_t deliveryTag,
                     bool multiple = false)
    {
        sc.processReceived(
            rmqamqp::Message(rmqamqpt::Method(rmqamqpt::BasicMethod(
                rmqamqpt::BasicNack(deliveryTag, multiple)))));
    }
    void receiveMessage(rmqamqp::SendChannel& sc, const rmqt::Message& msg)
    {
        sc.processReceived(rmqamqp::Message(msg));
    }
    void receiveReturn(rmqamqp::SendChannel& sc,
                       const rmqamqpt::Constants::AMQPReplyCode replyCode,
                       const bsl::string& replyText,
                       const bsl::string& exchange   = "foo",
                       const bsl::string& routingKey = "key")
    {
        sc.processReceived(rmqamqp::Message(
            rmqamqpt::Method(rmqamqpt::BasicMethod(rmqamqpt::BasicReturn(
                replyCode, replyText, exchange, routingKey)))));
    }

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
};

class SendChannelTests : public SendChannelTestsBase {
  public:
    SendChannelTests()
    : SendChannelTestsBase()
    , d_sendChannel(bsl::make_shared<rmqamqp::SendChannel>(
          d_topology,
          d_exchange,
          d_onAsyncWrite,
          d_retryHandler,
          d_metricPublisher,
          TEST_VHOST,
          d_timerFactory->createWithCallback(&noopHungTimerCallback),
          &noopHungCallback))
    {
        d_sendChannel->setCallback(d_onConfirm);
        ON_CALL(d_callback, onAsyncWrite(_, _))
            .WillByDefault(InvokeArgument<1>());
    }

    bsl::shared_ptr<rmqamqp::SendChannel> d_sendChannel;
};
} // namespace

TEST_F(SendChannelTests, Breathing)
{
    // Constructs a SendChannel in the SendChannelTests constructor
}

TEST_F(SendChannelTests, SetProducerConfirmation)
{
    openAndSendTopology(*d_sendChannel);

    selectExpectations();
    queueDeclareReply(*d_sendChannel);

    EXPECT_THAT(d_sendChannel->state(), Eq(rmqamqp::Channel::AWAITING_REPLY));
    selectOkReply(*d_sendChannel);
    EXPECT_THAT(d_sendChannel->state(), Eq(rmqamqp::Channel::READY));
}

TEST_F(SendChannelTests, PublishMessage)
{
    startupExpectations(*d_sendChannel);
    // The channel should now be in READY state

    rmqt::Message message;

    publishMessage(*d_sendChannel, message);
}

TEST_F(SendChannelTests, AckReceivedRemovesMessageFromStore)
{
    startupExpectations(*d_sendChannel);

    rmqt::Message msg;

    publishMessage(*d_sendChannel, msg);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(1));

    EXPECT_CALL(d_mockConfirm,
                conf(msg,
                     d_routingKey,
                     rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK)))
        .Times(1);

    receiveAck(*d_sendChannel, 1);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(0));
}

TEST_F(SendChannelTests, NAckReceivedRemovesMessageFromStore)
{
    startupExpectations(*d_sendChannel);

    rmqt::Message msg;

    publishMessage(*d_sendChannel, msg);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(1));

    EXPECT_CALL(d_mockConfirm,
                conf(msg,
                     d_routingKey,
                     rmqt::ConfirmResponse(rmqt::ConfirmResponse::REJECT)))
        .Times(1);

    receiveNack(*d_sendChannel, 1);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(0));
}

TEST_F(SendChannelTests, BasicReturnReceivedRemovesMessageFromStore)
{
    startupExpectations(*d_sendChannel);

    rmqt::Message msg;

    publishMessage(*d_sendChannel, msg);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(1));

    rmqamqpt::Constants::AMQPReplyCode replyCode =
        rmqamqpt::Constants::NO_ROUTE;
    bsl::string replyText = "no-route";

    EXPECT_CALL(
        d_mockConfirm,
        conf(msg, d_routingKey, rmqt::ConfirmResponse(replyCode, replyText)))
        .Times(1);

    receiveReturn(*d_sendChannel, replyCode, replyText);
    receiveMessage(*d_sendChannel, msg);
    receiveAck(*d_sendChannel, 1);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(0));
}

TEST_F(SendChannelTests, BasicReturnMultiAckCorrectlyResponds)
{
    startupExpectations(*d_sendChannel);

    rmqt::Message msg, msg2;

    publishMessage(*d_sendChannel, msg);

    publishMessage(*d_sendChannel, msg2);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(2));

    rmqamqpt::Constants::AMQPReplyCode replyCode =
        rmqamqpt::Constants::NO_ROUTE;
    bsl::string replyText = "no-route";

    Sequence seq;

    EXPECT_CALL(
        d_mockConfirm,
        conf(msg, d_routingKey, rmqt::ConfirmResponse(replyCode, replyText)))
        .Times(1)
        .InSequence(seq);

    EXPECT_CALL(d_mockConfirm,
                conf(msg2,
                     d_routingKey,
                     rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK)))
        .Times(1)
        .InSequence(seq);

    receiveReturn(*d_sendChannel, replyCode, replyText);
    receiveMessage(*d_sendChannel, msg);
    receiveAck(*d_sendChannel, 2, true);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(0));
}

TEST_F(SendChannelTests, AckReceivedRemovesMessageFromStoreOutOfOrder)
{
    startupExpectations(*d_sendChannel);

    publishMessages(*d_sendChannel, 4);

    EXPECT_CALL(d_mockConfirm,
                conf(_, _, rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK)))
        .Times(4);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(4));

    receiveAck(*d_sendChannel, 3); // ack message 3

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(3));

    receiveAck(*d_sendChannel, 2);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(2));

    receiveAck(*d_sendChannel, 1);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(1));

    receiveAck(*d_sendChannel, 4);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(0));
}

TEST_F(SendChannelTests,
       DoubleAckDoesntChangeAnything) // maybe we want to close the channel if
                                      // we're being strict
{
    startupExpectations(*d_sendChannel);

    publishMessages(*d_sendChannel, 3);

    EXPECT_CALL(d_mockConfirm,
                conf(_, _, rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK)))
        .Times(1);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(3));

    receiveAck(*d_sendChannel, 1);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(2));

    receiveAck(*d_sendChannel, 1);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(2));
}

TEST_F(SendChannelTests, MultiAckReceivedRemovesMessagesFromStore)
{
    startupExpectations(*d_sendChannel);

    EXPECT_CALL(d_mockConfirm,
                conf(_, _, rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK)))
        .Times(4);

    publishMessages(*d_sendChannel, 12);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(12));

    receiveAck(*d_sendChannel, 4, true);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(12 - 4));
}

TEST_F(SendChannelTests, MultiAckMixReceivedRemovesMessagesFromStore)
{
    startupExpectations(*d_sendChannel);

    publishMessages(*d_sendChannel, 12);

    EXPECT_CALL(d_mockConfirm,
                conf(_, _, rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK)))
        .Times(4);

    receiveAck(*d_sendChannel, 4, true);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(12 - 4));

    EXPECT_CALL(d_mockConfirm,
                conf(_, _, rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK)))
        .Times(1);

    receiveAck(*d_sendChannel, 5);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(7));

    EXPECT_CALL(d_mockConfirm,
                conf(_, _, rmqt::ConfirmResponse(rmqt::ConfirmResponse::ACK)))
        .Times(8 - 5);
    receiveAck(*d_sendChannel, 8, true);

    EXPECT_THAT(d_sendChannel->inFlight(), Eq(12 - 8));
}

TEST_F(SendChannelTests, PublishPendingMessagesWhenChannelIsReady)
{
    rmqt::Message message;
    d_sendChannel->publishMessage(
        message, d_routingKey, rmqt::Mandatory::RETURN_UNROUTABLE);
    // Channel is not yet ready, the message should be added to the pending
    // message list
    expectMessageNotPublished(message);
    Mock::VerifyAndClearExpectations(&d_callback);
    expectMessages(message, 1);
    startupExpectations(*d_sendChannel);
    // Now we expect message to be published
}

TEST_F(SendChannelTests, MessagesArePublishedAfterReconnecting)
{
    rmqt::Message message;
    startupExpectations(*d_sendChannel);
    Mock::VerifyAndClearExpectations(&d_callback);
    d_sendChannel->reset(true);
    d_sendChannel->publishMessage(
        message, d_routingKey, rmqt::Mandatory::RETURN_UNROUTABLE);
    expectMessageNotPublished(message);
    Mock::VerifyAndClearExpectations(&d_callback);

    expectMessages(message, 1);
    startupExpectations(*d_sendChannel);
}

TEST_F(SendChannelTests, MessagesArePublishedAfterFlow)
{
    rmqt::Message message;
    startupExpectations(*d_sendChannel);
    Mock::VerifyAndClearExpectations(&d_callback);

    rmqamqpt::ChannelFlow channelFlowFalse(false);
    rmqamqpt::ChannelFlow channelFlowTrue(true);

    expectFlowOkMessage(false);
    d_sendChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(channelFlowFalse))));
    d_sendChannel->publishMessage(
        message, d_routingKey, rmqt::Mandatory::RETURN_UNROUTABLE);
    expectMessageNotPublished(message);
    Mock::VerifyAndClearExpectations(&d_callback);

    expectMessages(message, 1);
    expectFlowOkMessage(true);
    d_sendChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(channelFlowTrue))));
}

TEST_F(SendChannelTests, MessagesAreNotPublishedAfterFlowBeforeReady)
{
    rmqt::Message message;
    openAndSendTopology(*d_sendChannel);

    rmqamqpt::ChannelFlow channelFlowFalse(false);
    rmqamqpt::ChannelFlow channelFlowTrue(true);

    expectFlowOkMessage(false);
    d_sendChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(channelFlowFalse))));
    d_sendChannel->publishMessage(
        message, d_routingKey, rmqt::Mandatory::RETURN_UNROUTABLE);

    // after being unblocked we still don't want to send the message
    expectMessageNotPublished(message);
    expectFlowOkMessage(true);
    d_sendChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(channelFlowTrue))));
}

TEST_F(SendChannelTests, PublishMessageCounterMetricWhenChannelIsNotReady)
{
    expectCounterMetric("client_sent_messages", 1);
    expectNoCounterMetric("published_messages");

    rmqt::Message msg;
    d_sendChannel->publishMessage(
        msg, d_routingKey, rmqt::Mandatory::RETURN_UNROUTABLE);

    Mock::VerifyAndClearExpectations(&*d_metricPublisher);

    expectMessageNotPublished(msg);
    Mock::VerifyAndClearExpectations(&d_callback);
    expectMessages(msg, 1);
    startupExpectations(*d_sendChannel);
}

TEST_F(SendChannelTests, PublishMessageCounterMetricWhenChannelIsReady)
{
    startupExpectations(*d_sendChannel);

    int cnt = 5;

    expectCounterMetric("client_sent_messages", 1, cnt);
    expectCounterMetric("published_messages", 1, cnt);

    publishMessages(*d_sendChannel, cnt);
}

TEST_F(SendChannelTests, PublishAndConfirmPushesMetric)
{
    startupExpectations(*d_sendChannel);

    rmqt::Message msg;
    publishMessage(*d_sendChannel, msg);

    EXPECT_CALL(d_mockConfirm, conf(_, _, _)).Times(1);
    expectAnyDistributionMetric("confirm_latency");
    receiveAck(*d_sendChannel, 1);
}

TEST_F(SendChannelTests, PublishMandatoryFalseMessage)
{
    Mock::VerifyAndClearExpectations(&d_callback);
    startupExpectations(*d_sendChannel);

    const bool mandatory = false;

    rmqt::Message message;
    expectMessages(message, 1, mandatory);

    d_sendChannel->publishMessage(
        message, d_routingKey, rmqt::Mandatory::DISCARD_UNROUTABLE);
}

TEST_F(SendChannelTests, PublishMandatoryMessage)
{
    Mock::VerifyAndClearExpectations(&d_callback);
    startupExpectations(*d_sendChannel);

    const bool mandatory = true;

    rmqt::Message message;
    expectMessages(message, 1, mandatory);

    d_sendChannel->publishMessage(
        message, d_routingKey, rmqt::Mandatory::RETURN_UNROUTABLE);
}
