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

#include <rmqamqp_receivechannel.h>

#include <rmqamqp_channeltests.t.h>
#include <rmqamqp_framer.h>
#include <rmqamqp_metrics.h>

#include <rmqamqpt_basicconsumeok.h>
#include <rmqamqpt_basicmethod.h>
#include <rmqamqpt_basicqos.h>
#include <rmqamqpt_basicqosok.h>
#include <rmqamqpt_method.h>
#include <rmqio_retryhandler.h>
#include <rmqt_consumerack.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_envelope.h>
#include <rmqt_fieldvalue.h>
#include <rmqt_message.h>
#include <rmqt_topology.h>
#include <rmqtestutil_mockmetricpublisher.h>

#include <bdlf_bind.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_optional.h>
#include <bsl_stdexcept.h>

#include <bsl_utility.h>
#include <bsl_vector.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqp;
using namespace ::testing;

namespace {
const char* TEST_VHOST = "vhostname";

void noopHungTimerCallback(rmqio::Timer::InterruptReason) {}

template <typename MethodClassT, typename MethodTypeT>
const MethodTypeT& MethodGetter(const bsl::shared_ptr<rmqamqp::Message>& msg)
{
    if (msg->is<rmqamqpt::Method>() &&
        msg->the<rmqamqpt::Method>().is<MethodClassT>()) {
        const MethodClassT& mc =
            msg->the<rmqamqpt::Method>().template the<MethodClassT>();
        if (mc.template is<MethodTypeT>()) {
            return mc.template the<MethodTypeT>();
        }
    }
    throw bsl::runtime_error("MethodMatcher: No Match");
}

template <typename MethodClassT, typename MethodTypeT>
bool MethodMatcher(const bsl::shared_ptr<rmqamqp::Message>& msg)
{
    return (msg->is<rmqamqpt::Method>() &&
            msg->the<rmqamqpt::Method>().is<MethodClassT>() &&
            msg->the<rmqamqpt::Method>()
                .the<MethodClassT>()
                .template is<MethodTypeT>());
}
MATCHER_P(EXPECT_QOSPREFETCH_IS, pf, "Prefetch isn't " + PrintToString(pf))
{
    try {
        return MethodGetter<rmqamqpt::BasicMethod, rmqamqpt::BasicQoS>(arg)
                   .prefetchCount() == pf;
    }
    catch (...) {
        return false;
    }
}

MATCHER_P(EXPECT_CONSUME_IS,
          consume,
          "Consume Isn't: " + PrintToString(consume))
{
    try {
        return MethodGetter<rmqamqpt::BasicMethod, rmqamqpt::BasicConsume>(
                   arg) == consume;
    }
    catch (...) {
        return false;
    }
}

MATCHER_P(EXPECT_CANCEL_IS, cancel, "Cancel Isn't: " + PrintToString(cancel))
{
    try {
        return MethodGetter<rmqamqpt::BasicMethod, rmqamqpt::BasicCancel>(
                   arg) == cancel;
    }
    catch (...) {
        return false;
    }
}

using namespace bdlf::PlaceHolders;

class ReceiveChannelTests : public rmqamqp::ChannelTests {
  public:
    StrictMock<rmqamqp::ReceiveChannel::MessageCallback> d_onNewMessage;
    bsl::string d_consumerTag;
    bsl::shared_ptr<rmqt::ConsumerAckQueue> d_ackQueue;
    bsl::shared_ptr<rmqtestutil::MockMetricPublisher> d_metricPublisher;
    bsl::vector<bsl::pair<bsl::string, bsl::string> > d_vhostTag;

    ReceiveChannelTests()
    : rmqamqp::ChannelTests()
    , d_onNewMessage(bdlf::BindUtil::bind(&rmqamqp::Callback::onNewMessage,
                                          &d_callback,
                                          _1))
    , d_consumerTag("test-tag")
    , d_ackQueue(bsl::make_shared<rmqt::ConsumerAckQueue>())
    , d_metricPublisher(bsl::make_shared<rmqtestutil::MockMetricPublisher>())
    {
        d_vhostTag.push_back(bsl::pair<bsl::string, bsl::string>(
            rmqamqp::Metrics::VHOST_TAG, TEST_VHOST));
    }

    void ackOrNackMessage(rmqamqp::ReceiveChannel& rc,
                          const rmqt::Envelope& id,
                          rmqt::ConsumerAck::Type type)
    {
        bsl::shared_ptr<rmqt::ConsumerAckBatch> batch =
            bsl::make_shared<rmqt::ConsumerAckBatch>();
        batch->addAck(rmqt::ConsumerAck(id, type));
        d_ackQueue->pushBack(batch);
        rc.consumeAckBatchFromQueue();
    }

    void ackMessage(rmqamqp::ReceiveChannel& rc, const rmqt::Envelope& id)
    {
        ackOrNackMessage(rc, id, rmqt::ConsumerAck::ACK);
    }

    void nackMessage(rmqamqp::ReceiveChannel& rc,
                     const rmqt::Envelope& id,
                     bool requeue = true)
    {
        ackOrNackMessage(rc,
                         id,
                         requeue ? rmqt::ConsumerAck::REQUEUE
                                 : rmqt::ConsumerAck::REJECT);
    }

    void setupConsumerNoReply(rmqamqp::ReceiveChannel& rc,
                              const bsl::string& consumerTag = "")
    {
        const bsl::string tag =
            consumerTag.empty() ? d_consumerTag : consumerTag;
        rc.consume(d_queue, d_onNewMessage, tag);
    }

    void consumerReply(rmqamqp::ReceiveChannel& rc,
                       const bsl::string& consumerTag = "")
    {
        rc.processReceived(rmqamqp::Message(
            rmqamqpt::Method(rmqamqpt::BasicMethod(rmqamqpt::BasicConsumeOk(
                consumerTag.empty() ? d_consumerTag : consumerTag)))));
    }

    void setupConsumer(rmqamqp::ReceiveChannel& rc,
                       const bsl::string& consumerTag = "")
    {
        EXPECT_CALL(d_callback, onAsyncWrite(_, _))
            .WillOnce(InvokeArgument<1>());
        setupConsumerNoReply(rc, consumerTag);
        EXPECT_THAT(rc.state(), Eq(rmqamqp::Channel::AWAITING_REPLY));
        consumerReply(rc, consumerTag);
        EXPECT_THAT(rc.state(), Eq(rmqamqp::Channel::READY));
    }

    void qosOkReply(rmqamqp::ReceiveChannel& rc)
    {
        rc.processReceived(rmqamqp::Message(
            rmqamqpt::Method(rmqamqpt::BasicMethod(rmqamqpt::BasicQoSOk()))));
    }

    void qosExpectations()
    {
        EXPECT_CALL(
            d_callback,
            onAsyncWrite(Pointee(MethodMsgTypeEq(rmqamqpt::Method(
                             rmqamqpt::BasicMethod(rmqamqpt::BasicQoS())))),
                         _))
            .WillOnce(InvokeArgument<1>()); // qos
    }

    void cancelOkReply(rmqamqp::ReceiveChannel& rc, const bsl::string& tag)
    {
        rc.processReceived(rmqamqp::Message(rmqamqpt::Method(
            rmqamqpt::BasicMethod(rmqamqpt::BasicCancelOk(tag)))));
    }

    void cancelExpectations()
    {
        EXPECT_CALL(
            d_callback,
            onAsyncWrite(Pointee(MethodMsgTypeEq(rmqamqpt::Method(
                             rmqamqpt::BasicMethod(rmqamqpt::BasicCancel())))),
                         _))
            .WillOnce(InvokeArgument<1>()); // cancel
    }

    void makeReady(rmqamqp::ReceiveChannel& rc)
    {
        openAndSendTopology(rc);
        qosExpectations();
        queueDeclareReply(rc);

        EXPECT_THAT(rc.state(), Eq(rmqamqp::Channel::AWAITING_REPLY));

        qosOkReply(rc);
    }

    void receiveMessage(rmqamqp::ReceiveChannel& rc,
                        uint64_t deliveryTag,
                        const bsl::string& consumerTag = "")
    {
        EXPECT_CALL(d_callback, onNewMessage(_));
        rc.processReceived(rmqamqp::Message(
            rmqamqpt::Method(rmqamqpt::BasicMethod(rmqamqpt::BasicDeliver(
                consumerTag.empty() ? d_consumerTag : consumerTag,
                deliveryTag,
                false,
                "exchange",
                "routing-key")))));
        rc.processReceived(rmqamqp::Message(rmqt::Message()));
    }

    void ackExpectations(uint64_t deliveryTag, bool multiple = false)
    {
        EXPECT_CALL(
            d_callback,
            onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                             rmqamqpt::Method(rmqamqpt::BasicMethod(
                                 rmqamqpt::BasicAck(deliveryTag, multiple)))))),
                         _))
            .WillOnce(InvokeArgument<1>());
    }

    bsl::shared_ptr<ReceiveChannel> makeReceiveChannel(
        size_t prefetchCount                    = 100,
        bsl::optional<int64_t> consumerPriority = bsl::optional<int64_t>())
    {
        rmqt::ConsumerConfig consumerConfig(
            rmqt::ConsumerConfig::generateConsumerTag(), prefetchCount);
        consumerConfig.setConsumerPriority(consumerPriority);

        return bsl::make_shared<ReceiveChannel>(
            d_topology,
            d_onAsyncWrite,
            d_retryHandler,
            d_metricPublisher,
            consumerConfig,
            TEST_VHOST,
            d_ackQueue,
            d_timerFactory->createWithCallback(&noopHungTimerCallback),
            d_connErrorCb);
    }
};

} // namespace

TEST_F(ReceiveChannelTests, Breathing)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel();
}

TEST_F(ReceiveChannelTests, SetsQoSAfterTopologyLoaded)
{
    const size_t PREFETCH_COUNT = 50;

    bsl::shared_ptr<ReceiveChannel> receiveChannel =
        makeReceiveChannel(PREFETCH_COUNT);
    openAndSendTopology(*receiveChannel);

    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_QOSPREFETCH_IS(PREFETCH_COUNT), _))
        .WillOnce(InvokeArgument<1>());

    queueDeclareReply(*receiveChannel);

    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::AWAITING_REPLY));

    receiveChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::BasicMethod(rmqamqpt::BasicQoSOk()))));

    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));
}

TEST_F(ReceiveChannelTests, Consume)
{
    int64_t consumerPriority = 5;
    bsl::shared_ptr<ReceiveChannel> receiveChannel =
        makeReceiveChannel(100, consumerPriority);

    makeReady(*receiveChannel);

    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    rmqt::FieldTable consumeArguments;
    consumeArguments["x-priority"] = consumerPriority;
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                         "test-queue", d_consumerTag, consumeArguments)),
                     _))
        .WillOnce(InvokeArgument<1>());

    rmqt::Result<> result =
        receiveChannel->consume(d_queue, d_onNewMessage, d_consumerTag);

    EXPECT_TRUE(result);
    EXPECT_FALSE(receiveChannel->consumerIsActive());

    receiveChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
        rmqamqpt::BasicMethod(rmqamqpt::BasicConsumeOk(d_consumerTag)))));

    EXPECT_TRUE(receiveChannel->consumerIsActive());
}

TEST_F(ReceiveChannelTests, HandleReopen)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel();

    makeReady(*receiveChannel);

    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    setupConsumer(*receiveChannel);

    EXPECT_TRUE(receiveChannel->consumerIsActive());

    openExpectations();
    qosExpectations();

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(Pointee(MethodMsgTypeEq(rmqamqpt::Method(
                         rmqamqpt::BasicMethod(rmqamqpt::BasicConsume())))),
                     _))
        .WillOnce(InvokeArgument<1>()); // Consumer

    // Re-open the channel
    EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());

    receiveChannel->reset(true);

    openOkReply(*receiveChannel);
    queueDeclareReply(*receiveChannel);
    qosOkReply(*receiveChannel);
    consumerReply(*receiveChannel);

    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));
}
TEST_F(ReceiveChannelTests, BadQueueHandleThrows)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel();
    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    rmqt::QueueHandle badQueueHandle(bsl::make_shared<rmqt::Queue>());
    rmqt::Result<> result =
        receiveChannel->consume(badQueueHandle, d_onNewMessage, d_consumerTag);
    EXPECT_TRUE(!result);
}

TEST_F(ReceiveChannelTests, UnknownConsumeTag)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel();
    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                                 "test-queue", d_consumerTag)),
                             _))
        .WillOnce(InvokeArgument<1>());

    receiveChannel->consume(d_queue, d_onNewMessage, d_consumerTag);

    EXPECT_FALSE(receiveChannel->consumerIsActive());

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(Pointee(MethodMsgTypeEq(rmqamqpt::Method(
                         rmqamqpt::ChannelMethod(rmqamqpt::ChannelClose())))),
                     _))
        .WillOnce(InvokeArgument<1>());

    receiveChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
        rmqamqpt::BasicMethod(rmqamqpt::BasicConsumeOk("unknown")))));

    EXPECT_THAT(receiveChannel->state(),
                Eq(rmqamqp::Channel::CHANNEL_CLOSE_SENT));

    // Re-open the channel
    EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(Pointee(MessageEq(rmqamqp::Message(rmqamqpt::Method(
                         rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen()))))),
                     _))
        .WillOnce(InvokeArgument<1>());

    receiveChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(rmqamqpt::ChannelCloseOk()))));

    EXPECT_FALSE(receiveChannel->consumerIsActive());
}

TEST_F(ReceiveChannelTests, Deliver)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));
    setupConsumer(*receiveChannel);

    receiveChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::BasicMethod(rmqamqpt::BasicDeliver(
            d_consumerTag, 1, false, "exchange", "routing-key")))));
    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));

    EXPECT_CALL(d_callback, onNewMessage(_));
    receiveChannel->processReceived(rmqamqp::Message(rmqt::Message()));
    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));
}

TEST_F(ReceiveChannelTests, UnexpectedDeliver)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));
    setupConsumer(*receiveChannel);

    receiveChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::BasicMethod(rmqamqpt::BasicDeliver(
            "unexpected-consumer", 1, false, "exchange", "routing-key")))));
    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(
            Pointee(MessageEq(rmqamqp::Message(
                rmqamqpt::Method(rmqamqpt::ChannelMethod(rmqamqpt::ChannelClose(
                    rmqamqpt::Constants::NOT_FOUND, "Invalid ConsumerTag")))))),
            _))
        .WillOnce(InvokeArgument<1>());
    receiveChannel->processReceived(rmqamqp::Message(rmqt::Message()));

    EXPECT_THAT(receiveChannel->state(),
                Eq(rmqamqp::Channel::CHANNEL_CLOSE_SENT));
}

TEST_F(ReceiveChannelTests, ServerCancels)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));
    setupConsumer(*receiveChannel, "consumer1");
    EXPECT_TRUE(receiveChannel->consumerIsActive());

    receiveMessage(*receiveChannel, 1, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(Pointee(MessageEq(rmqamqp::Message(rmqamqpt::Method(
                         rmqamqpt::ChannelMethod(rmqamqpt::ChannelClose(
                             rmqamqpt::Constants::REPLY_SUCCESS,
                             "Closing Channel due to consumer cancel",
                             rmqamqpt::Constants::NO_CLASS,
                             rmqamqpt::Constants::NO_METHOD)))))),
                     _))
        .WillOnce(InvokeArgument<1>());

    receiveChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
        rmqamqpt::BasicMethod(rmqamqpt::BasicCancel("consumer1")))));

    EXPECT_THAT(receiveChannel->state(),
                Eq(rmqamqp::Channel::CHANNEL_CLOSE_SENT));

    // Ensure the channel re-opens when the broker sends CloseOk
    EXPECT_CALL(*d_retryHandler, retry(_));

    receiveChannel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(rmqamqpt::ChannelCloseOk()))));

    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::CLOSED));
}

TEST_F(ReceiveChannelTests, AckMessage)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    setupConsumer(*receiveChannel, "consumer1");
    EXPECT_TRUE(receiveChannel->consumerIsActive());

    const uint64_t deliveryTag = 40;

    rmqt::Envelope id_ack(deliveryTag,
                          receiveChannel->lifetimeId(),
                          "consumer1",
                          "exchange",
                          "routing-key",
                          false);
    receiveMessage(*receiveChannel, id_ack.deliveryTag(), "consumer1");
    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));
    ackExpectations(id_ack.deliveryTag());
    ackMessage(*receiveChannel, id_ack);
    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));
}

TEST_F(ReceiveChannelTests, NackMessage)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    setupConsumer(*receiveChannel, "consumer1");
    EXPECT_TRUE(receiveChannel->consumerIsActive());

    const uint64_t deliveryTag = 41;
    rmqt::Envelope id_nack(deliveryTag,
                           receiveChannel->lifetimeId(),
                           "consumer1",
                           "exchange",
                           "routing-key",
                           false);
    receiveMessage(*receiveChannel, id_nack.deliveryTag(), "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    rmqamqpt::BasicNack basicNack(id_nack.deliveryTag(), true);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::BasicMethod(basicNack))))),
                     _))
        .WillOnce(InvokeArgument<1>());

    nackMessage(*receiveChannel, id_nack);
    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));
}

TEST_F(ReceiveChannelTests, WrongChannelLifetimeId)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);
    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    setupConsumer(*receiveChannel, "consumer1");
    EXPECT_TRUE(receiveChannel->consumerIsActive());

    uint64_t deliveryTag = 40;
    rmqt::Envelope id_ack(deliveryTag,
                          receiveChannel->lifetimeId(),
                          "consumer1",
                          "exchange",
                          "routing-key",
                          false);
    receiveMessage(*receiveChannel, id_ack.deliveryTag(), "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    rmqamqpt::BasicAck basicAck(id_ack.deliveryTag(), false);
    {
        InSequence s;
        EXPECT_CALL(d_callback, onAsyncWrite(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(InvokeArgument<1>());
        EXPECT_CALL(d_callback,
                    onAsyncWrite(::testing::Pointee(MessageEq(
                                     rmqamqp::Message(rmqamqpt::Method(
                                         rmqamqpt::BasicMethod(basicAck))))),
                                 _))
            .Times(0);
    }

    // Re-open the channel
    EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());

    // channel's lifetimeId is changed
    receiveChannel->reset(true);
    openOkReply(*receiveChannel);
    queueDeclareReply(*receiveChannel);
    qosOkReply(*receiveChannel);
    consumerReply(*receiveChannel, "consumer1");

    ackMessage(*receiveChannel, id_ack);

    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));
}

TEST_F(ReceiveChannelTests, AckMsgsHavingDiffLifetimeIdSameDeliveryTag)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);
    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    setupConsumer(*receiveChannel, "consumer1");
    EXPECT_TRUE(receiveChannel->consumerIsActive());

    uint64_t deliveryTag = 40;
    rmqt::Envelope id_ack(deliveryTag,
                          receiveChannel->lifetimeId(),
                          "consumer1",
                          "exchange",
                          "routing-key",
                          false);
    receiveMessage(*receiveChannel, id_ack.deliveryTag(), "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    rmqamqpt::BasicAck basicAck(id_ack.deliveryTag(), false);
    {
        InSequence s;
        EXPECT_CALL(d_callback, onAsyncWrite(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(InvokeArgument<1>());
        EXPECT_CALL(d_callback,
                    onAsyncWrite(::testing::Pointee(MessageEq(
                                     rmqamqp::Message(rmqamqpt::Method(
                                         rmqamqpt::BasicMethod(basicAck))))),
                                 _))
            .Times(0);
    }

    // Re-open the channel
    EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());

    // channel's lifetimeId is changed
    receiveChannel->reset(true);
    openOkReply(*receiveChannel);
    queueDeclareReply(*receiveChannel);
    qosOkReply(*receiveChannel);
    consumerReply(*receiveChannel, "consumer1");

    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));
    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));

    rmqt::Envelope id_ack2(deliveryTag,
                           receiveChannel->lifetimeId(),
                           "consumer1",
                           "exchange",
                           "routing-key",
                           false);
    receiveMessage(*receiveChannel, id_ack2.deliveryTag(), "consumer1");
    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    ackMessage(*receiveChannel, id_ack);
    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    rmqamqpt::BasicAck basicAck2(id_ack2.deliveryTag(), false);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::BasicMethod(basicAck2))))),
                     _))
        .WillOnce(InvokeArgument<1>());
    ackMessage(*receiveChannel, id_ack2);
    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));
}

TEST_F(ReceiveChannelTests, AckMessagePublishesMetric)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 40;
    rmqt::Envelope id_ack(deliveryTag,
                          receiveChannel->lifetimeId(),
                          "consumer1",
                          "exchange",
                          "routingKey",
                          false);
    receiveMessage(*receiveChannel, id_ack.deliveryTag(), "consumer1");

    ackExpectations(deliveryTag);

    EXPECT_CALL(
        *d_metricPublisher,
        publishDistribution(bsl::string("acknowledge_latency"), _, d_vhostTag));

    ackMessage(*receiveChannel, id_ack);
}

TEST_F(ReceiveChannelTests, ReceivedMsgPublishesMetric)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    EXPECT_CALL(
        *d_metricPublisher,
        publishCounter(bsl::string("received_messages"), 1, d_vhostTag));

    const uint64_t deliveryTag = 40;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");
}

TEST_F(ReceiveChannelTests, Cancel)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));
    size_t lifetimeId = receiveChannel->lifetimeId();

    cancelExpectations();
    rmqt::Future<> wereCancelled = receiveChannel->cancel();

    EXPECT_THAT(lifetimeId, Eq(receiveChannel->lifetimeId()));
    EXPECT_THAT(!!wereCancelled.tryResult(), Eq(false));

    cancelOkReply(*receiveChannel, "consumer1");

    EXPECT_THAT(!!wereCancelled.tryResult(), Eq(true));
    EXPECT_THAT(lifetimeId, Eq(receiveChannel->lifetimeId()));
    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    rmqt::Envelope env(
        deliveryTag, lifetimeId, "consumer1", "exchange", "routing-key", false);
    ackExpectations(deliveryTag);
    ackMessage(*receiveChannel, env);

    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));

    // ack a message received earlier
}

TEST_F(ReceiveChannelTests, CancelCalled2x)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));
    size_t lifetimeId = receiveChannel->lifetimeId();

    cancelExpectations();
    rmqt::Future<> wereCancelled = receiveChannel->cancel();

    EXPECT_THAT(lifetimeId, Eq(receiveChannel->lifetimeId()));
    EXPECT_THAT(!!wereCancelled.tryResult(), Eq(false));

    rmqt::Future<> wereCancelled2x = receiveChannel->cancel();

    cancelOkReply(*receiveChannel, "consumer1");

    EXPECT_THAT(!!wereCancelled.tryResult(), Eq(true));
    EXPECT_THAT(!!wereCancelled2x.tryResult(), Eq(true));
    EXPECT_THAT(lifetimeId, Eq(receiveChannel->lifetimeId()));
    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    rmqt::Envelope env(
        deliveryTag, lifetimeId, "consumer1", "exchange", "routing-key", false);
    ackExpectations(deliveryTag);
    ackMessage(*receiveChannel, env);

    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));
}

TEST_F(ReceiveChannelTests, CancelFail)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);

    rmqt::Future<> wereCancelled = receiveChannel->cancel();
    rmqt::Result<> result        = wereCancelled.tryResult();
    EXPECT_FALSE(result);
    EXPECT_THAT(result.returnCode(), Ne(rmqt::TIMEOUT));
}

TEST_F(ReceiveChannelTests, DrainRegular)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));
    size_t lifetimeId = receiveChannel->lifetimeId();

    cancelExpectations();
    rmqt::Future<> wereCancelled = receiveChannel->cancel();
    cancelOkReply(*receiveChannel, "consumer1");

    EXPECT_TRUE(wereCancelled.blockResult());
    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    rmqt::Future<> wereDrained = receiveChannel->drain();
    rmqt::Result<> result      = wereDrained.tryResult();
    EXPECT_FALSE(result);
    EXPECT_THAT(result.returnCode(), Eq(rmqt::TIMEOUT));

    rmqt::Envelope env(
        deliveryTag, lifetimeId, "consumer1", "exchange", "routing-key", false);

    ackExpectations(deliveryTag);
    ackMessage(*receiveChannel, env);

    EXPECT_TRUE(wereDrained.tryResult());

    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));

    result = wereDrained.tryResult();
    EXPECT_TRUE(result);
    // ack a message received earlier
}

TEST_F(ReceiveChannelTests, DrainFail)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    rmqt::Future<> wereDrained = receiveChannel->drain();
    rmqt::Result<> result      = wereDrained.tryResult();
    EXPECT_FALSE(result);
    EXPECT_THAT(result.returnCode(), Ne(rmqt::TIMEOUT));
}

TEST_F(ReceiveChannelTests, CancelFutureCancelledOnReset)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    cancelExpectations();
    rmqt::Future<> wereCancelled = receiveChannel->cancel();
    EXPECT_THAT(wereCancelled.tryResult().returnCode(), Eq(rmqt::TIMEOUT));
    receiveChannel->reset();
    EXPECT_FALSE(wereCancelled.tryResult());
    EXPECT_THAT(wereCancelled.tryResult().returnCode(), Ne(rmqt::TIMEOUT));
}

TEST_F(ReceiveChannelTests, DrainFutureCancelledOnReset)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    cancelExpectations();
    receiveChannel->cancel();
    cancelOkReply(*receiveChannel, "consumer1");

    rmqt::Future<> wereDrained = receiveChannel->drain();
    EXPECT_THAT(wereDrained.tryResult().returnCode(), Eq(rmqt::TIMEOUT));
    receiveChannel->reset();
    EXPECT_FALSE(wereDrained.tryResult());
    EXPECT_THAT(wereDrained.tryResult().returnCode(), Ne(rmqt::TIMEOUT));
}

TEST_F(ReceiveChannelTests, CancelFutureCancelledOnDestruct)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    cancelExpectations();
    rmqt::Future<> wereCancelled = receiveChannel->cancel();
    EXPECT_THAT(wereCancelled.tryResult().returnCode(), Eq(rmqt::TIMEOUT));
    receiveChannel->reset();
    EXPECT_FALSE(wereCancelled.tryResult());
    EXPECT_THAT(wereCancelled.tryResult().returnCode(), Ne(rmqt::TIMEOUT));
}

TEST_F(ReceiveChannelTests, DrainFutureCancelledOnDestruct)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    setupConsumer(*receiveChannel, "consumer1");

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    cancelExpectations();
    receiveChannel->cancel();
    cancelOkReply(*receiveChannel, "consumer1");

    rmqt::Future<> wereDrained = receiveChannel->drain();
    EXPECT_THAT(wereDrained.tryResult().returnCode(), Eq(rmqt::TIMEOUT));
    receiveChannel->reset();
    EXPECT_FALSE(wereDrained.tryResult());
    EXPECT_THAT(wereDrained.tryResult().returnCode(), Ne(rmqt::TIMEOUT));
}

TEST_F(ReceiveChannelTests, CancelConsumerBeforeTopologyConfirmed)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel();

    openAndSendTopology(*receiveChannel);

    EXPECT_THAT(receiveChannel->state(),
                Eq(rmqamqp::Channel::DECLARING_TOPOLOGY));

    // We must not start consuming before channel is ready
    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                                 "test-queue", d_consumerTag)),
                             _))
        .Times(0);

    rmqt::Result<> result =
        receiveChannel->consume(d_queue, d_onNewMessage, d_consumerTag);

    EXPECT_TRUE(result);
    EXPECT_FALSE(receiveChannel->consumerIsActive());

    // User cancels consumer before topology confirmed by broker -- we must not
    // send Basic.Cancel because the consumer is not started
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(EXPECT_CANCEL_IS(rmqamqpt::BasicCancel(d_consumerTag)), _))
        .Times(0);

    rmqt::Future<> cancelFuture = receiveChannel->cancel();

    // Now, after cancel() has been called, we expect consume() not to be called
    // when channel becomes ready
    // The remaining steps of makeReady()
    qosExpectations();
    queueDeclareReply(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::AWAITING_REPLY));
    qosOkReply(*receiveChannel);
}

TEST_F(ReceiveChannelTests, CancelConsumerBeforeConsumerStarted)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel();

    makeReady(*receiveChannel);

    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                                 "test-queue", d_consumerTag)),
                             _))
        .WillOnce(InvokeArgument<1>());

    rmqt::Result<> result =
        receiveChannel->consume(d_queue, d_onNewMessage, d_consumerTag);

    EXPECT_TRUE(result);
    EXPECT_FALSE(receiveChannel->consumerIsActive());

    // We must not cancel the consumer before the broker sends BasicConsumeOk
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(EXPECT_CANCEL_IS(rmqamqpt::BasicCancel(d_consumerTag)), _))
        .Times(0);

    rmqt::Future<> cancelFuture = receiveChannel->cancel();

    // After BasicConsumeOk is received, we want to immediately cancel the
    // consumer
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(EXPECT_CANCEL_IS(rmqamqpt::BasicCancel(d_consumerTag)), _))
        .WillOnce(InvokeArgument<1>());

    receiveChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
        rmqamqpt::BasicMethod(rmqamqpt::BasicConsumeOk(d_consumerTag)))));

    cancelOkReply(*receiveChannel, d_consumerTag);

    EXPECT_FALSE(receiveChannel->consumerIsActive());
}

TEST_F(ReceiveChannelTests, AckBatchesInFlightDuringResetGetLocked)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);
    EXPECT_THAT(receiveChannel->state(), Eq(rmqamqp::Channel::READY));

    setupConsumer(*receiveChannel, "consumer1");
    EXPECT_TRUE(receiveChannel->consumerIsActive());

    const size_t lifetime_id = receiveChannel->lifetimeId();

    bsl::shared_ptr<rmqt::ConsumerAckBatch> batch =
        bsl::make_shared<rmqt::ConsumerAckBatch>();

    {
        // Ack one

        const uint64_t deliveryTag = 40;

        rmqt::Envelope id_ack(deliveryTag,
                              receiveChannel->lifetimeId(),
                              "consumer1",
                              "exchange",
                              "routing-key",
                              false);

        batch->addAck(rmqt::ConsumerAck(id_ack, rmqt::ConsumerAck::ACK));
        d_ackQueue->pushBack(batch);
    }

    receiveChannel->reset(true);

    // This is actually a no-op, but demonstrates that this function must be
    // called after onReset for this issue to present
    receiveChannel->consumeAckBatchFromQueue();

    EXPECT_THAT(receiveChannel->lifetimeId(), Ne(lifetime_id));
    {
        // Ack two
        const uint64_t deliveryTag = 1;

        rmqt::Envelope id_ack(deliveryTag,
                              receiveChannel->lifetimeId(),
                              "consumer1",
                              "exchange",
                              "routing-key",
                              false);

        // Attempt to add another ack to ensure the AckBatch created above is
        // locked This indicates to the ConsumerImpl that it needs to create a
        // new AckBatch.
        EXPECT_THAT(
            batch->addAck(rmqt::ConsumerAck(id_ack, rmqt::ConsumerAck::ACK)),
            Eq(false));
    }

    EXPECT_THAT(receiveChannel->inFlight(), Eq(0));
}

class ReceiveChannelHungTests : public ReceiveChannelTests {};

TEST_F(ReceiveChannelHungTests, HungQoSOk)
{
    // Channel is setup
    bsl::shared_ptr<ReceiveChannel> rc = makeReceiveChannel();

    openAndSendTopology(*rc);

    // Expect that when we send the queue declare, we get a QoS message.
    qosExpectations();
    queueDeclareReply(*rc);

    // If the channel is never sent a QoSOk, the hung callback should be called.
    const int HUNG_TIMEOUT_SECONDS = 65;

    EXPECT_CALL(d_callback, onHungTimerCallback());
    d_timerFactory->step_time(bsls::TimeInterval(HUNG_TIMEOUT_SECONDS));
}

TEST_F(ReceiveChannelHungTests, HungConsumeOk)
{
    // Channel is setup
    bsl::shared_ptr<ReceiveChannel> rc = makeReceiveChannel();

    makeReady(*rc);

    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                                 "test-queue", d_consumerTag)),
                             _))
        .WillOnce(InvokeArgument<1>());

    rmqt::Result<> result = rc->consume(d_queue, d_onNewMessage, d_consumerTag);

    // If the channel is never sent a ConsumeOk, the hung callback should be
    // called.
    const int HUNG_TIMEOUT_SECONDS = 65;

    EXPECT_CALL(d_callback, onHungTimerCallback());
    d_timerFactory->step_time(bsls::TimeInterval(HUNG_TIMEOUT_SECONDS));
}
