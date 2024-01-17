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

using namespace bdlf::PlaceHolders;

class ConsumerTests : public rmqamqp::ChannelTests {
  public:
    StrictMock<rmqamqp::ReceiveChannel::MessageCallback> d_onNewMessage;
    bsl::string d_consumerTag;
    bsl::shared_ptr<rmqt::ConsumerAckQueue> d_ackQueue;
    bsl::shared_ptr<rmqtestutil::MockMetricPublisher> d_metricPublisher;
    bsl::vector<bsl::pair<bsl::string, bsl::string> > d_vhostTag;

    ConsumerTests()
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

TEST_F(ConsumerTests, Consume)
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

TEST_F(ConsumerTests, UnknownConsumeTag)
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

TEST_F(ConsumerTests, Cancel)
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

TEST_F(ConsumerTests, CancelCalled2x)
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

TEST_F(ConsumerTests, CancelFail)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);

    rmqt::Future<> wereCancelled = receiveChannel->cancel();
    rmqt::Result<> result        = wereCancelled.tryResult();
    EXPECT_FALSE(result);
    EXPECT_THAT(result.returnCode(), Ne(rmqt::TIMEOUT));
}

TEST_F(ConsumerTests, ResumeCancelled)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    (*receiveChannel).open();
    queueDeclareReply(*receiveChannel);
    qosOkReply(*receiveChannel);

    (*receiveChannel).consume(d_queue, d_onNewMessage, "consumer1");
    consumerReply(*receiveChannel, "consumer1");
    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                                 "test-queue", "consumer1")),
                             _))
        .WillRepeatedly(InvokeArgument<1>());

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

    rmqt::Future<> wereResumed = receiveChannel->resume();

    EXPECT_THAT(!!wereResumed.tryResult(), Eq(false));
    EXPECT_FALSE(receiveChannel->consumerIsActive());

    receiveChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
        rmqamqpt::BasicMethod(rmqamqpt::BasicConsumeOk("consumer1")))));

    EXPECT_TRUE(receiveChannel->consumerIsActive());
    EXPECT_THAT(!!wereResumed.tryResult(), Eq(true));
}

TEST_F(ConsumerTests, ResumeCancelling)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    (*receiveChannel).open();
    queueDeclareReply(*receiveChannel);
    qosOkReply(*receiveChannel);

    (*receiveChannel).consume(d_queue, d_onNewMessage, "consumer1");
    consumerReply(*receiveChannel, "consumer1");
    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                                 "test-queue", "consumer1")),
                             _))
        .WillRepeatedly(InvokeArgument<1>());

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));
    size_t lifetimeId = receiveChannel->lifetimeId();

    cancelExpectations();
    rmqt::Future<> wereCancelled = receiveChannel->cancel();

    EXPECT_THAT(lifetimeId, Eq(receiveChannel->lifetimeId()));
    EXPECT_THAT(!!wereCancelled.tryResult(), Eq(false));

    rmqt::Future<> wereResumed = receiveChannel->resume();

    EXPECT_THAT(!!wereResumed.tryResult(), Eq(false));
    EXPECT_FALSE(receiveChannel->consumerIsActive());

    receiveChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
        rmqamqpt::BasicMethod(rmqamqpt::BasicConsumeOk("consumer1")))));

    EXPECT_TRUE(receiveChannel->consumerIsActive());
    EXPECT_THAT(!!wereResumed.tryResult(), Eq(true));
}

TEST_F(ConsumerTests, ResumeActive)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    (*receiveChannel).open();
    queueDeclareReply(*receiveChannel);
    qosOkReply(*receiveChannel);

    (*receiveChannel).consume(d_queue, d_onNewMessage, "consumer1");
    consumerReply(*receiveChannel, "consumer1");
    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                                 "test-queue", "consumer1")),
                             _))
        .WillRepeatedly(InvokeArgument<1>());

    const uint64_t deliveryTag = 88;
    receiveMessage(*receiveChannel, deliveryTag, "consumer1");

    EXPECT_THAT(receiveChannel->inFlight(), Eq(1));

    rmqt::Future<> wereResumed = receiveChannel->resume();

    EXPECT_THAT(!!wereResumed.tryResult(), Eq(false));
    EXPECT_TRUE(receiveChannel->consumerIsActive());
}

TEST_F(ConsumerTests, ResumeCalled2x)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    (*receiveChannel).open();
    queueDeclareReply(*receiveChannel);
    qosOkReply(*receiveChannel);

    (*receiveChannel).consume(d_queue, d_onNewMessage, "consumer1");
    consumerReply(*receiveChannel, "consumer1");
    EXPECT_CALL(d_callback,
                onAsyncWrite(EXPECT_CONSUME_IS(rmqamqpt::BasicConsume(
                                 "test-queue", "consumer1")),
                             _))
        .WillRepeatedly(InvokeArgument<1>());

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

    rmqt::Future<> wereResumed = receiveChannel->resume();
    EXPECT_THAT(!!wereResumed.tryResult(), Eq(false));

    rmqt::Future<> wereResumed2x = receiveChannel->resume();
    EXPECT_THAT(!!wereResumed2x.tryResult(), Eq(false));

    EXPECT_FALSE(receiveChannel->consumerIsActive());

    receiveChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
        rmqamqpt::BasicMethod(rmqamqpt::BasicConsumeOk("consumer1")))));

    EXPECT_TRUE(receiveChannel->consumerIsActive());
    EXPECT_THAT(!!wereResumed.tryResult(), Eq(true));
    EXPECT_THAT(!!wereResumed2x.tryResult(), Eq(true));
}

TEST_F(ConsumerTests, ResumeFail)
{
    bsl::shared_ptr<ReceiveChannel> receiveChannel = makeReceiveChannel(1);

    makeReady(*receiveChannel);

    rmqt::Future<> wereCancelled = receiveChannel->resume();
    rmqt::Result<> result        = wereCancelled.tryResult();
    EXPECT_FALSE(result);
    EXPECT_THAT(result.returnCode(), Ne(rmqt::TIMEOUT));
}

TEST_F(ConsumerTests, DrainRegular)
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

TEST_F(ConsumerTests, DrainFail)
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

TEST_F(ConsumerTests, CancelFutureCancelledOnReset)
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

TEST_F(ConsumerTests, DrainFutureCancelledOnReset)
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

TEST_F(ConsumerTests, CancelFutureCancelledOnDestruct)
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

TEST_F(ConsumerTests, DrainFutureCancelledOnDestruct)
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
