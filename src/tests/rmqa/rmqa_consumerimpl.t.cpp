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

#include <rmqa_consumerimpl.h>
#include <rmqa_tracingconsumerimpl.h>

#include <rmqa_messageguard.h>

#include <rmqp_consumertracing.h>

#include <rmqtestutil_mockchannel.t.h>
#include <rmqtestutil_mockeventloop.t.h>
#include <rmqtestutil_savethreadid.h>

#include <rmqt_consumerackbatch.h>
#include <rmqt_envelope.h>
#include <rmqt_queue.h>
#include <rmqt_simpleendpoint.h>

#include <bdlf_bind.h>
#include <bslmt_threadutil.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace ::testing;
using namespace bdlf::PlaceHolders;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.CONSUMERIMPL.T")

class ConsumerCallback {
  public:
    virtual void onMessage(rmqp::MessageGuard&) = 0;
};

class MockConsumerCallback : public ConsumerCallback {
  public:
    MOCK_METHOD1(onMessage, void(rmqp::MessageGuard&));
};

class MockConsumerTracing : public rmqp::ConsumerTracing {
  public:
    struct MockContext : rmqp::ConsumerTracing::Context {};
    MockConsumerTracing()
    {
        ON_CALL(*this, create(_, _, _))
            .WillByDefault(Return(bsl::make_shared<MockContext>()));
    }

    MOCK_CONST_METHOD3(
        create,
        bsl::shared_ptr<Context>(
            const rmqp::MessageGuard& messageGuard,
            const bsl::string& queueName,
            const bsl::shared_ptr<const rmqt::Endpoint>& endpoint));
};
enum ConsumerType { CONSUMER, TRACING_CONSUMER };

struct MessageHolder : public rmqamqp::Message {
    // This 'class' exists to allow a 'Message' to moved around as copies
    // within bdlf::BindUtil::bind.  'bind' attempts to move arguments within
    // it.  'Message' is a variant containing classes containing variants, and
    // some of those classes don't have a normal set of copy / move c'tors, but
    // rather a 'template <class T> Ctor(const T&)' c'tors, which get totally
    // confused if a moved object is passed to them.  By containing 'Message'
    // in 'MessageHolder' and not providing a move c'tor, we ensure that the
    // base class is just copied and never moved around, which it can cope
    // with.

    // CREATORS
    MessageHolder(const rmqamqp::Message& message)
    : rmqamqp::Message(message)
    {
    }

    MessageHolder(const MessageHolder& original)
    : rmqamqp::Message(*static_cast<const rmqamqp::Message*>(&original))
    {
    }
};

ACTION(CallAckOnMessageGuard) { arg0.ack(); }
ACTION(ExecuteItem) { arg0(); }
} // namespace

class ConsumerImplTests : public TestWithParam<ConsumerType> {
  public:
    struct PrintParamName {
        template <class ParamType>
        bsl::string operator()(const TestParamInfo<ParamType>& info) const
        {
            switch (info.param) {
                case TRACING_CONSUMER:
                    return "TracingConsumer";
                default:
                    return "Consumer";
            }
        }
    };

  protected:
    rmqt::QueueHandle d_queue;
    bsl::string d_consumerTag;
    bdlmt::ThreadPool d_threadPool;
    rmqtestutil::MockEventLoop d_eventLoop;
    bsl::shared_ptr<rmqt::ConsumerAckQueue> d_ackQueue;
    bsl::shared_ptr<rmqtestutil::MockReceiveChannel> d_channel;

    MockConsumerCallback d_mockCallback;
    bsl::shared_ptr<rmqp::Consumer::ConsumerFunc> d_callback;
    bsl::shared_ptr<MockConsumerTracing> d_tracing;
    bsl::shared_ptr<rmqa::ConsumerImpl::Factory> d_factory;

    ConsumerImplTests()
    : d_queue(bsl::make_shared<rmqt::Queue>("test"))
    , d_consumerTag("test consumer")
    , d_threadPool(bslmt::ThreadAttributes(), 0, 5, 5)
    , d_eventLoop()
    , d_ackQueue(bsl::make_shared<rmqt::ConsumerAckQueue>())
    , d_channel(bsl::make_shared<rmqtestutil::MockReceiveChannel>(d_ackQueue))
    , d_mockCallback()
    , d_callback(bsl::make_shared<rmqp::Consumer::ConsumerFunc>(
          bdlf::BindUtil::bind(&ConsumerCallback::onMessage,
                               &d_mockCallback,
                               _1)))
    , d_tracing(bsl::make_shared<MockConsumerTracing>())
    , d_factory(paramPicker(GetParam()))

    {
        d_threadPool.start();
        ON_CALL(d_eventLoop, postImpl(_)).WillByDefault(ExecuteItem());
    }
    bsl::shared_ptr<rmqa::ConsumerImpl::Factory> paramPicker(ConsumerType ct)
    {
        switch (ct) {
            case TRACING_CONSUMER:
                return bsl::make_shared<rmqa::TracingConsumerImpl::Factory>(
                    bsl::make_shared<rmqt::SimpleEndpoint>("example-hostname",
                                                           "example-vhost"),
                    d_tracing);
            default:
                return bsl::make_shared<rmqa::ConsumerImpl::Factory>();
        }
    }
};

TEST_P(ConsumerImplTests, ItsAlive)
{
    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(d_channel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);
}

TEST_P(ConsumerImplTests, MessageTriggersClientCallback)
{
    rmqamqp::ReceiveChannel::MessageCallback injectMessage;
    EXPECT_CALL(*d_channel, consume(_, _, _))
        .WillOnce(DoAll(SaveArg<1>(&injectMessage), Return(rmqt::Result<>())));

    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(d_channel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);
    consumer->start();

    bsl::shared_ptr<bsl::vector<uint8_t> > data =
        bsl::make_shared<bsl::vector<uint8_t> >(5);

    rmqt::Message message(data);

    uint64_t threadid = 0;

    EXPECT_CALL(d_mockCallback, onMessage(_))
        .WillOnce(Invoke(bdlf::BindUtil::bind(&rmqtestutil::saveThreadId,
                                              bsl::ref(threadid))));

    injectMessage(
        message,
        rmqt::Envelope(0, 0, "consumerTag", "exchange", "routing-key", false));

    d_threadPool.stop();
    bsl::string threadName;
    bslmt::ThreadUtil::getThreadName(&threadName);

    BALL_LOG_INFO << "Main Thread name: " << threadName;

    EXPECT_THAT(threadid, Ne(bslmt::ThreadUtil::selfIdAsInt()));
}

TEST_P(ConsumerImplTests, MessageTriggersChannelAck)
{
    rmqamqp::ReceiveChannel::MessageCallback injectMessage;
    EXPECT_CALL(*d_channel, consume(_, _, _))
        .WillOnce(DoAll(SaveArg<1>(&injectMessage), Return(rmqt::Result<>())));

    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(d_channel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);
    consumer->start();

    bsl::shared_ptr<bsl::vector<uint8_t> > data =
        bsl::make_shared<bsl::vector<uint8_t> >(5);
    bsl::shared_ptr<MockConsumerTracing::MockContext> tracingContext(
        bsl::make_shared<MockConsumerTracing::MockContext>());
    {
        rmqt::Message message(data);
        if (GetParam() == TRACING_CONSUMER) {
            EXPECT_CALL(*d_tracing, create(_, _, _))
                .WillOnce(Return(tracingContext));
        }
        EXPECT_CALL(d_mockCallback, onMessage(_))
            .WillOnce(CallAckOnMessageGuard());

        EXPECT_CALL(*d_channel, consumeAckBatchFromQueue());

        injectMessage(
            message,
            rmqt::Envelope(
                0, 0, "consumerTag", "exchange", "routing-key", false));
    }

    d_threadPool.stop();
}

TEST_P(ConsumerImplTests, Cancel)
{
    rmqt::Future<>::Pair fakeyCancelFuture = rmqt::Future<>::make();

    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(d_channel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);

    EXPECT_CALL(*d_channel, cancel())
        .WillOnce(Return(fakeyCancelFuture.second));
    rmqt::Future<> future = consumer->cancel();
    bool result           = future.tryResult();
    EXPECT_THAT(result, Eq(false));
    fakeyCancelFuture.first(rmqt::Result<>());
    result = future.tryResult();

    EXPECT_THAT(result, Eq(true));
}

TEST_P(ConsumerImplTests, Drain)
{
    rmqt::Future<>::Pair fakeyDrainFuture = rmqt::Future<>::make();

    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(d_channel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);

    EXPECT_CALL(*d_channel, drain()).WillOnce(Return(fakeyDrainFuture.second));
    rmqt::Future<> future = consumer->drain();
    bool result           = future.tryResult();
    EXPECT_THAT(result, Eq(false));
    fakeyDrainFuture.first(rmqt::Result<>());
    result = future.tryResult();

    EXPECT_THAT(result, Eq(true));
}

TEST_P(ConsumerImplTests, UpdateCallback)
{
    bsl::shared_ptr<rmqtestutil::MockUpdateReceiveChannel>
        mockUpdateReceiveChannel =
            bsl::make_shared<rmqtestutil::MockUpdateReceiveChannel>(d_ackQueue);
    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(mockUpdateReceiveChannel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));

    rmqt::Future<> future = consumer->updateTopologyAsync(topologyUpdate);

    EXPECT_FALSE(future.tryResult());

    rmqamqpt::QueueBindOk queueBindOkMethod;
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateReceiveChannel,
                             MessageHolder(rmqamqp::Message(rmqamqpt::Method(
                                 rmqamqpt::QueueMethod(queueBindOkMethod))))));
    EXPECT_TRUE(future.blockResult());
}

TEST_P(ConsumerImplTests, BindUnbind)
{
    bsl::shared_ptr<rmqtestutil::MockUpdateReceiveChannel>
        mockUpdateReceiveChannel =
            bsl::make_shared<rmqtestutil::MockUpdateReceiveChannel>(d_ackQueue);
    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(mockUpdateReceiveChannel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));

    bsl::shared_ptr<rmqt::QueueUnbinding> queueUnbindingPtr =
        bsl::make_shared<rmqt::QueueUnbinding>(
            exchangePtr, queuePtr, "bindKey");
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueUnbindingPtr));

    rmqt::Future<> future = consumer->updateTopologyAsync(topologyUpdate);

    rmqamqpt::QueueBindOk queueBindOkMethod;
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateReceiveChannel,
                             MessageHolder(rmqamqp::Message(rmqamqpt::Method(
                                 rmqamqpt::QueueMethod(queueBindOkMethod))))));
    EXPECT_FALSE(future.tryResult());

    rmqamqpt::QueueUnbindOk queueUnbindOkMethod;
    d_eventLoop.post(bdlf::BindUtil::bind(
        &rmqamqp::SendChannel::processReceived,
        mockUpdateReceiveChannel,
        MessageHolder(rmqamqp::Message(
            rmqamqpt::Method(rmqamqpt::QueueMethod(queueUnbindOkMethod))))));
    EXPECT_TRUE(future.blockResult());
}

TEST_P(ConsumerImplTests, DestructionInitiatesChannelClose)
{
    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(d_channel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);
    consumer->start();

    EXPECT_CALL(*d_channel, gracefulClose());
    consumer.reset();
    Mock::VerifyAndClearExpectations(&d_eventLoop);

    d_threadPool.stop();
}

TEST_P(ConsumerImplTests, UpdateCallbackFromTwoThreadsAtOnce)
{
    bsl::shared_ptr<rmqtestutil::MockUpdateReceiveChannel>
        mockUpdateReceiveChannel =
            bsl::make_shared<rmqtestutil::MockUpdateReceiveChannel>(d_ackQueue);
    bsl::shared_ptr<rmqa::ConsumerImpl> consumer =
        d_factory->create(mockUpdateReceiveChannel,
                          bsl::ref(d_queue),
                          d_callback,
                          d_consumerTag,
                          bsl::ref(d_threadPool),
                          bsl::ref(d_eventLoop),
                          d_ackQueue);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    EXPECT_CALL(d_eventLoop, postImpl(_)).Times(2);
    rmqt::Future<> future1 = consumer->updateTopologyAsync(topologyUpdate);
    rmqt::Future<> future2 = consumer->updateTopologyAsync(topologyUpdate);
    Mock::VerifyAndClearExpectations(&d_eventLoop);

    rmqamqpt::QueueBindOk queueBindOkMethod;
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateReceiveChannel,
                             MessageHolder(rmqamqp::Message(rmqamqpt::Method(
                                 rmqamqpt::QueueMethod(queueBindOkMethod))))));
    EXPECT_TRUE(future1.blockResult());
    EXPECT_FALSE(future2.tryResult());
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateReceiveChannel,
                             MessageHolder(rmqamqp::Message(rmqamqpt::Method(
                                 rmqamqpt::QueueMethod(queueBindOkMethod))))));
    EXPECT_TRUE(future2.blockResult());
}

// We need to stick to INSTANTIATE_TEST_CASE_P for a while longer
// But we do want to build with -Werror in our CI
#pragma GCC diagnostic warning "-Wdeprecated-declarations"

INSTANTIATE_TEST_CASE_P(AllMembers,
                        ConsumerImplTests,
                        Values(CONSUMER, TRACING_CONSUMER),
                        ConsumerImplTests::PrintParamName());
