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

#include <rmqamqp_channel.h>

#include <rmqamqp_framer.h>
#include <rmqamqp_metrics.h>
#include <rmqamqpt_queuedeclare.h>
#include <rmqamqpt_queuedeclareok.h>
#include <rmqio_retryhandler.h>
#include <rmqt_envelope.h>
#include <rmqt_result.h>
#include <rmqt_topology.h>
#include <rmqtestutil_mockmetricpublisher.h>
#include <rmqtestutil_mockretryhandler.t.h>

#include <bdlf_bind.h>
#include <bsls_assert.h>

#include <bsl_memory.h>
#include <bsls_keyword.h>

#include <bsl_functional.h>
#include <bsl_utility.h>
#include <bsl_vector.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

namespace {
const char* TEST_VHOST = "vhostname";
void noOpOnError(const bsl::string&, int) {}
void dummyHungTimerCallback(rmqio::Timer::InterruptReason)
{
    BSLS_ASSERT(
        !"This timer is only called when Channel hasn't properly set up "
         "its own");
}

void noopHungCallback() {}
} // namespace

class Callback {
  public:
    virtual ~Callback() {}
    virtual void
    onAsyncWrite(const bsl::shared_ptr<rmqamqp::Message>&,
                 const rmqio::Connection::SuccessWriteCallback&) = 0;

    virtual void onHungTimerCallback() = 0;
};

class MockCallback : public Callback {
  public:
    MOCK_METHOD2(onAsyncWrite,
                 void(const bsl::shared_ptr<rmqamqp::Message>&,
                      const rmqio::Connection::SuccessWriteCallback&));

    MOCK_METHOD0(onHungTimerCallback, void());
};

class ChannelTestImpl : public rmqamqp::Channel {
  public:
    ChannelTestImpl(
        const rmqt::Topology& topology,
        const rmqamqp::Channel::AsyncWriteCallback& onAsyncWrite,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& timerFactory,
        const rmqamqp::Channel::HungChannelCallback& connErrorCb =
            &noopHungCallback)
    : rmqamqp::Channel(
          topology,
          onAsyncWrite,
          retryHandler,
          metricPublisher,
          TEST_VHOST,
          timerFactory->createWithCallback(&dummyHungTimerCallback),
          connErrorCb)
    , d_timerFactory(timerFactory)
    {
    }

    MOCK_METHOD1(processBasicMethod, void(const rmqamqpt::BasicMethod& basic));
    MOCK_METHOD0(processFailures, void());
    MOCK_CONST_METHOD0(inFlight, size_t());
    MOCK_CONST_METHOD0(lifetimeId, size_t());
    virtual const char* channelType() const BSLS_KEYWORD_OVERRIDE
    {
        return "mockchannel";
    };

    bsl::string channelDebugName() const BSLS_KEYWORD_OVERRIDE
    {
        return "Mock Channel Debug Name";
    }

    void setState(rmqamqp::Channel::State state) { updateState(state); }

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
};

using namespace bdlf::PlaceHolders;

class ChannelTests : public ::testing::Test {
  public:
    MockCallback d_callback;
    StrictMock<rmqamqp::Channel::AsyncWriteCallback> d_onAsyncWrite;
    bsl::shared_ptr<rmqtestutil::MockRetryHandler> d_retryHandler;
    rmqt::Topology d_topology;
    rmqt::ErrorCallback d_errorCallback;
    bsl::shared_ptr<rmqtestutil::MockMetricPublisher> d_metricPublisher;
    bsl::vector<bsl::pair<bsl::string, bsl::string> > d_vhostTag;
    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
    StrictMock<rmqamqp::Channel::HungChannelCallback> d_connErrorCb;

    ChannelTests()
    : d_onAsyncWrite(
          bdlf::BindUtil::bind(&Callback::onAsyncWrite, &d_callback, _1, _2))
    , d_retryHandler(new rmqtestutil::MockRetryHandler())
    , d_topology()
    , d_errorCallback(noOpOnError)
    , d_metricPublisher(bsl::make_shared<rmqtestutil::MockMetricPublisher>())
    , d_timerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    , d_connErrorCb(
          bdlf::BindUtil::bind(&Callback::onHungTimerCallback, &d_callback))
    {
        d_topology.queues.push_back(bsl::make_shared<rmqt::Queue>("test"));
        d_vhostTag.push_back(bsl::pair<bsl::string, bsl::string>(
            rmqamqp::Metrics::VHOST_TAG, TEST_VHOST));
    }

    bsl::shared_ptr<ChannelTestImpl> makeChannel(const rmqt::Topology& topology)
    {
        return bsl::make_shared<ChannelTestImpl>(topology,
                                                 d_onAsyncWrite,
                                                 d_retryHandler,
                                                 d_metricPublisher,
                                                 d_timerFactory,
                                                 d_connErrorCb);
    }

    bsl::shared_ptr<ChannelTestImpl>
    makeOpeningChannel(const rmqt::Topology& topology)
    {
        bsl::shared_ptr<ChannelTestImpl> channel = makeChannel(topology);
        ensureChannelOpen(channel);

        return channel;
    }

    void ensureChannelOpen(const bsl::shared_ptr<ChannelTestImpl>& channel);
};

MATCHER_P(MessageEq, expected, "")
{
    uint16_t irrelevant = 123;
    bsl::vector<rmqamqpt::Frame> actualFrames, expectedFrames;
    rmqamqp::Framer framer;
    framer.makeFrames(&actualFrames, irrelevant, arg);
    framer.makeFrames(&expectedFrames, irrelevant, expected);
    return (actualFrames == expectedFrames);
}

void ChannelTests::ensureChannelOpen(
    const bsl::shared_ptr<ChannelTestImpl>& channel)
{
    rmqamqpt::ChannelOpen openMethod;

    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::ChannelMethod(openMethod))))),
                             _))
        .RetiresOnSaturation();

    channel->open();
}

TEST_F(ChannelTests, OpenChannel)
{
    bsl::shared_ptr<ChannelTestImpl> channel = makeOpeningChannel(d_topology);
}

TEST_F(ChannelTests, GracefulCloseChannel)
{
    bsl::shared_ptr<ChannelTestImpl> mockChannel = makeChannel(d_topology);
    rmqamqpt::ChannelClose closeMethod(rmqamqpt::Constants::REPLY_SUCCESS,
                                       "Closing Channel");

    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::ChannelMethod(closeMethod))))),
                             _));

    mockChannel->gracefulClose();
}

TEST_F(ChannelTests, closingProcessCloseOkReturnsCleanup)
{
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(d_topology);
    mockChannel->setState(rmqamqp::Channel::READY);

    rmqamqpt::ChannelClose closeMethod(rmqamqpt::Constants::REPLY_SUCCESS,
                                       "Closing Channel");

    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::ChannelMethod(closeMethod))))),
                             _))
        .WillOnce(InvokeArgument<1>());

    mockChannel->gracefulClose();

    rmqamqpt::ChannelCloseOk closeOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(closeOkMethod)))),
                Eq(rmqamqp::Channel::CLEANUP));
}

TEST_F(ChannelTests, ReceiveCloseMethodReOpens)
{
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(d_topology);

    mockChannel->setState(rmqamqp::Channel::READY);

    rmqamqpt::ChannelCloseOk closeOkMethod;

    {
        InSequence s;
        EXPECT_CALL(
            d_callback,
            onAsyncWrite(
                ::testing::Pointee(MessageEq(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(closeOkMethod))))),
                _))
            .WillOnce(InvokeArgument<1>());

        // Re-open the channel
        EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());

        EXPECT_CALL(
            d_callback,
            onAsyncWrite(
                ::testing::Pointee(MessageEq(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen()))))),
                _))
            .WillOnce(InvokeArgument<1>());
    }

    d_vhostTag.push_back(bsl::pair<bsl::string, bsl::string>(
        rmqamqp::Metrics::CHANNELTYPE_TAG, "mockchannel"));
    EXPECT_CALL(
        *d_metricPublisher,
        publishCounter(bsl::string("channel_disconnected"), _, d_vhostTag));

    rmqamqpt::ChannelClose closeMethod(rmqamqpt::Constants::REPLY_SUCCESS,
                                       "Closing Channel");
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(closeMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::CHANNEL_OPEN_SENT));
}

TEST_F(ChannelTests, ReceiveOpenOkMethod)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);

    rmqamqpt::ChannelOpenOk openOkMethod;

    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::READY));
}

TEST_F(ChannelTests, QueueDeclare)
{
    rmqt::Topology topology;

    topology.queues.push_back(bsl::make_shared<rmqt::Queue>(
        "test-queue", false, false, true, rmqt::FieldTable()));

    bsl::shared_ptr<ChannelTestImpl> mockChannel = makeOpeningChannel(topology);

    rmqt::FieldTable args;
    rmqamqpt::QueueDeclare queueDeclare(
        "test-queue", false, true, false, false, false, args);
    rmqamqpt::ChannelOpenOk openOkMethod;

    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::QueueMethod(queueDeclare))))),
                             _));
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
}

TEST_F(ChannelTests, QueueDeclareOk)
{
    rmqt::Topology topology;

    topology.queues.push_back(bsl::make_shared<rmqt::Queue>(
        "test-queue-1", false, false, false, rmqt::FieldTable()));
    topology.queues.push_back(bsl::make_shared<rmqt::Queue>(
        "test-queue-2", false, false, false, rmqt::FieldTable()));

    bsl::shared_ptr<ChannelTestImpl> mockChannel = makeOpeningChannel(topology);

    rmqt::FieldTable args;

    rmqamqpt::QueueDeclare queueDeclare1(
        "test-queue-1", false, false, false, false, false, args);
    rmqamqpt::QueueDeclareOk declareOk1("test-queue-1", 0, 0);

    rmqamqpt::QueueDeclare queueDeclare2(
        "test-queue-2", false, false, false, false, false, args);

    rmqamqpt::QueueDeclareOk declareOk2("test-queue-2", 0, 0);
    rmqamqpt::ChannelOpenOk openOkMethod;

    // Expect that when the OpenOk comes back, we declare queue1 & 2.
    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::QueueMethod(queueDeclare1))))),
                             _));
    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::QueueMethod(queueDeclare2))))),
                             _));

    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    // Expect that receiving the first Queue.DeclareOk sends leaves the state in
    // DECLARING_TOPOLOGY
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::QueueMethod(declareOk1)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::DECLARING_TOPOLOGY));

    // Expect that receiving the second Queue.DeclareOk moves the state to READY
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::QueueMethod(declareOk2)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::READY));
}

TEST_F(ChannelTests, ResetTrueReopens)
{
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(d_topology);

    mockChannel->setState(rmqamqp::Channel::READY);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(
            ::testing::Pointee(MessageEq(rmqamqp::Message(rmqamqpt::Method(
                rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen()))))),
            _))
        .WillOnce(InvokeArgument<1>());

    // Re-open the channel
    EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());

    mockChannel->reset(true);

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::CHANNEL_OPEN_SENT));
}

TEST_F(ChannelTests, ResetWillTriggerTopologyDeclare)
{
    rmqt::Topology topology;
    topology.queues.push_back(bsl::make_shared<rmqt::Queue>(
        "test-queue", false, false, true, rmqt::FieldTable()));

    bsl::shared_ptr<ChannelTestImpl> mockChannel = makeOpeningChannel(topology);

    rmqt::FieldTable args;
    rmqamqpt::QueueDeclare queueDeclare(
        "test-queue", false, true, false, false, false, args);
    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::QueueMethod(queueDeclare))))),
                             _));

    rmqamqpt::ChannelOpenOk openOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::DECLARING_TOPOLOGY));

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(
            ::testing::Pointee(MessageEq(rmqamqp::Message(rmqamqpt::Method(
                rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen()))))),
            _))
        .WillOnce(InvokeArgument<1>());

    // Re-open the channel
    EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*mockChannel, processFailures());

    mockChannel->reset(true);

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::CHANNEL_OPEN_SENT));

    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::QueueMethod(queueDeclare))))),
                             _));
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::DECLARING_TOPOLOGY));

    rmqamqpt::QueueDeclareOk declareOk("test-queue", 0, 0);
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::QueueMethod(declareOk)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::READY));
}

TEST_F(ChannelTests, EmptyTopologyProgresses)
{
    rmqt::Topology emptyTopology;

    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);

    d_vhostTag.push_back(bsl::pair<bsl::string, bsl::string>(
        rmqamqp::Metrics::CHANNELTYPE_TAG, "mockchannel"));
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("channel_ready"), _, d_vhostTag));

    rmqamqpt::ChannelOpenOk openOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::READY));
}

TEST_F(ChannelTests, ExchangeDeclareOk)
{
    rmqt::Topology topology;

    topology.exchanges.push_back(bsl::make_shared<rmqt::Exchange>("exchange"));

    bsl::shared_ptr<ChannelTestImpl> mockChannel = makeOpeningChannel(topology);

    // Expect that receiving the OpenOk triggers the sending on Exchange.Declare

    rmqt::FieldTable args;
    rmqamqpt::ExchangeDeclare declare(
        "exchange", "direct", false, true, false, false, false, args);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::ExchangeMethod(declare))))),
                     _));

    rmqamqpt::ChannelOpenOk openOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::DECLARING_TOPOLOGY));

    rmqamqpt::ExchangeDeclareOk declareOk;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ExchangeMethod(declareOk)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::READY));
}

TEST_F(ChannelTests, PublishTopologyDeclareTimeMetric)
{
    rmqt::Topology emptyTopology;

    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);

    EXPECT_CALL(
        *d_metricPublisher,
        publishSummary(bsl::string("topology_declare_time"), _, d_vhostTag));

    rmqamqpt::ChannelOpenOk openOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::READY));
}

TEST_F(ChannelTests, waitForReadyWhenAlreadyReady)
{
    rmqt::Topology emptyTopology;

    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);
    mockChannel->setState(rmqamqp::Channel::READY);

    rmqt::Future<> ready = mockChannel->waitForReady();
    EXPECT_TRUE(ready.tryResult());
}

TEST_F(ChannelTests, waitForReady)
{
    rmqt::Topology emptyTopology;

    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);

    rmqt::Future<> ready = mockChannel->waitForReady();

    EXPECT_CALL(
        *d_metricPublisher,
        publishSummary(bsl::string("topology_declare_time"), _, d_vhostTag));

    rmqamqpt::ChannelOpenOk openOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_TRUE(ready.tryResult());
}

TEST_F(ChannelTests, waitForReady2x)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);

    rmqt::Future<> ready  = mockChannel->waitForReady();
    rmqt::Future<> ready2 = mockChannel->waitForReady();

    EXPECT_CALL(
        *d_metricPublisher,
        publishSummary(bsl::string("topology_declare_time"), _, d_vhostTag));

    rmqamqpt::ChannelOpenOk openOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_TRUE(ready.tryResult());
    EXPECT_TRUE(ready2.tryResult());
}

rmqt::Result<> holdSharedPtr(const bsl::shared_ptr<ChannelTestImpl>&,
                             const rmqt::Result<>&)
{
    return rmqt::Result<>();
}

TEST_F(ChannelTests, waitForReadyDoesntHoldSelfOwnership)
{
    rmqt::Topology emptyTopology;

    bsl::weak_ptr<ChannelTestImpl> weakChannel;
    {
        bsl::shared_ptr<ChannelTestImpl> mockChannel =
            makeOpeningChannel(emptyTopology);

        rmqt::Future<> readyChained = mockChannel->waitForReady().then<void>(
            bdlf::BindUtil::bind(&holdSharedPtr, mockChannel, _1));

        weakChannel = mockChannel;

        EXPECT_TRUE(weakChannel.lock());

        mockChannel.reset();
        EXPECT_TRUE(weakChannel.lock());
    }

    EXPECT_FALSE(weakChannel.lock());
}

TEST_F(ChannelTests, singleTopologyUpdate)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);
    mockChannel->setState(rmqamqp::Channel::READY);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");

    rmqt::FieldTable args;
    rmqamqpt::QueueBind queueBind("queue", "exchange", "bindKey", false, args);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::QueueMethod(queueBind))))),
                     _));
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    rmqt::Future<> future = mockChannel->updateTopology(topologyUpdate);

    rmqamqpt::QueueBindOk queueBindOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_TRUE(future.tryResult());
}

TEST_F(ChannelTests, emptyTopologyUpdate)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);
    mockChannel->setState(rmqamqp::Channel::READY);

    rmqt::TopologyUpdate topologyUpdate;
    rmqt::Future<> future = mockChannel->updateTopology(topologyUpdate);

    EXPECT_TRUE(future.tryResult());
}

TEST_F(ChannelTests, singleTopologyUpdateWithError)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel = makeChannel(emptyTopology);
    mockChannel->open();
    mockChannel->setState(rmqamqp::Channel::READY);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr; // empty Exchange pointer
    bsl::shared_ptr<rmqt::Queue> queuePtr;       // empty Queue pointer

    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");

    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));

    rmqt::Future<> future = mockChannel->updateTopology(topologyUpdate);

    EXPECT_FALSE(future.tryResult());
}

TEST_F(ChannelTests, doubleTopologyUpdate)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);
    mockChannel->setState(rmqamqp::Channel::READY);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");

    rmqt::FieldTable args;
    rmqamqpt::QueueBind queueBind("queue", "exchange", "bindKey", false, args);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::QueueMethod(queueBind))))),
                     _))
        .Times(2);
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    rmqt::Future<> future1 = mockChannel->updateTopology(topologyUpdate);
    rmqt::Future<> future2 = mockChannel->updateTopology(topologyUpdate);

    rmqamqpt::QueueBindOk queueBindOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_TRUE(future1.tryResult());
    EXPECT_FALSE(future2.tryResult());
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_TRUE(future2.tryResult());
}

TEST_F(ChannelTests, topologyUpdateHandledOnDisconnect)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);
    mockChannel->setState(rmqamqp::Channel::READY);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");

    rmqt::FieldTable args;
    rmqamqpt::QueueBind queueBind("queue", "exchange", "bindKey", false, args);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::QueueMethod(queueBind))))),
                     _))
        .Times(2);
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    rmqt::Future<> future = mockChannel->updateTopology(topologyUpdate);

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(
            ::testing::Pointee(MessageEq(rmqamqp::Message(rmqamqpt::Method(
                rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen()))))),
            _))
        .WillOnce(InvokeArgument<1>());

    // Re-open the channel
    EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*mockChannel, processFailures());

    mockChannel->reset(true);
    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::CHANNEL_OPEN_SENT));

    rmqamqpt::ChannelOpenOk openOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::READY));

    EXPECT_FALSE(future.tryResult());

    rmqamqpt::QueueBindOk queueBindOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_TRUE(future.tryResult());
}

TEST_F(ChannelTests, subsequentTopologyUpdateFails)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);
    mockChannel->setState(rmqamqp::Channel::READY);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");

    rmqt::FieldTable args;
    rmqamqpt::QueueBind queueBind("queue", "exchange", "bindKey", false, args);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::QueueMethod(queueBind))))),
                     _))
        .Times(2);
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    rmqt::Future<> future = mockChannel->updateTopology(topologyUpdate);

    rmqamqpt::QueueBindOk queueBindOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_TRUE(future.tryResult());

    rmqamqpt::QueueBind queueBind2(
        "queue", "exchange", "bindKey2", false, args);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::QueueMethod(queueBind2))))),
                     _))
        .Times(2);
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr2 =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey2");
    rmqt::TopologyUpdate topologyUpdate2;
    topologyUpdate2.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr2));
    future = mockChannel->updateTopology(topologyUpdate2);

    EXPECT_FALSE(future.tryResult());

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(
            ::testing::Pointee(MessageEq(rmqamqp::Message(rmqamqpt::Method(
                rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen()))))),
            _))
        .WillOnce(InvokeArgument<1>());

    // Re-open the channel
    EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*mockChannel, processFailures());

    mockChannel->reset(true);
    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::CHANNEL_OPEN_SENT));

    rmqamqpt::ChannelOpenOk openOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::DECLARING_TOPOLOGY));
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_THAT(mockChannel->state(), Eq(rmqamqp::Channel::READY));

    EXPECT_FALSE(future.tryResult());
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_TRUE(future.tryResult());
}

TEST_F(ChannelTests, topologyUpdatedInAwaitingReplyState)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);
    mockChannel->setState(rmqamqp::Channel::AWAITING_REPLY);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");

    rmqt::FieldTable args;
    rmqamqpt::QueueBind queueBind("queue", "exchange", "bindKey", false, args);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::QueueMethod(queueBind))))),
                     _));
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));

    rmqt::Future<> future = mockChannel->updateTopology(topologyUpdate);

    rmqamqpt::QueueBindOk queueBindOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));

    EXPECT_TRUE(future.tryResult());
}

TEST_F(ChannelTests, sendBindAndUnbind)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> mockChannel =
        makeOpeningChannel(emptyTopology);
    mockChannel->setState(rmqamqp::Channel::READY);

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");

    rmqt::FieldTable args;
    rmqamqpt::QueueBind queueBind("queue", "exchange", "bindKey", false, args);
    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::QueueMethod(queueBind))))),
                     _));
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    rmqt::Future<> future1 = mockChannel->updateTopology(topologyUpdate);

    rmqamqpt::QueueUnbind queueUnbind("queue", "exchange", "bindKey", args);
    EXPECT_CALL(d_callback,
                onAsyncWrite(
                    ::testing::Pointee(MessageEq(rmqamqp::Message(
                        rmqamqpt::Method(rmqamqpt::QueueMethod(queueUnbind))))),
                    _));
    bsl::shared_ptr<rmqt::QueueUnbinding> queueUnbindingPtr =
        bsl::make_shared<rmqt::QueueUnbinding>(
            exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate2;
    topologyUpdate2.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueUnbindingPtr));
    rmqt::Future<> future2 = mockChannel->updateTopology(topologyUpdate2);

    rmqamqpt::QueueBindOk queueBindOkMethod;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::QueueMethod(queueBindOkMethod)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_TRUE(future1.tryResult());
    EXPECT_FALSE(future2.tryResult());
    rmqamqpt::QueueUnbindOk queueUnbindOk;
    EXPECT_THAT(mockChannel->processReceived(rmqamqp::Message(
                    rmqamqpt::Method(rmqamqpt::QueueMethod(queueUnbindOk)))),
                Eq(rmqamqp::Channel::KEEP));
    EXPECT_TRUE(future2.tryResult());
}

TEST_F(ChannelTests, onWriteCompleteCbValidity)
{
    // Ensure that the onWriteComplete callback can be invoked after the Channel
    // is destructed.

    bsl::function<void()> callbackFn;

    {
        bsl::shared_ptr<ChannelTestImpl> channel = makeChannel(d_topology);

        EXPECT_CALL(d_callback, onAsyncWrite(_, _))
            .WillOnce(SaveArg<1>(&callbackFn));
        channel->open();
    }

    // This shouldn't segfault
    callbackFn();
}

class ChannelHungTests : public ChannelTests {};

TEST_F(ChannelHungTests, OpenOkTimeout)
{
    // If the broker takes too long sending an OpenOk, ensure the Channel
    // Hung Callback is called

    bsl::shared_ptr<ChannelTestImpl> channel = makeOpeningChannel(d_topology);

    // If the hung channel timeout passes
    // (Channel::k_HUNG_CHANNEL_TIMER_SEC) expect the hung channel callback
    const int HUNG_TIMEOUT_SECONDS = 65;

    EXPECT_CALL(d_callback, onHungTimerCallback());
    d_timerFactory->step_time(bsls::TimeInterval(HUNG_TIMEOUT_SECONDS));
}

TEST_F(ChannelHungTests, TopologyDeclareTimeout)
{
    // If the broker takes too long sending an OpenOk, ensure the Channel
    // Hung Callback is called

    bsl::shared_ptr<ChannelTestImpl> channel = makeOpeningChannel(d_topology);

    // Expect a Queue.Declare when OpenOk is received.
    rmqt::FieldTable args;
    rmqamqpt::QueueDeclare queueDeclare(
        "test", false, true, false, false, false, args);
    EXPECT_CALL(d_callback,
                onAsyncWrite(::testing::Pointee(
                                 MessageEq(rmqamqp::Message(rmqamqpt::Method(
                                     rmqamqpt::QueueMethod(queueDeclare))))),
                             _));

    rmqamqpt::ChannelOpenOk openOkMethod;
    channel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod))));

    // If the hung channel timeout passes
    // (Channel::k_HUNG_CHANNEL_TIMER_SEC) expect the hung channel callback
    const int HUNG_TIMEOUT_SECONDS = 65;

    EXPECT_CALL(d_callback, onHungTimerCallback());
    d_timerFactory->step_time(bsls::TimeInterval(HUNG_TIMEOUT_SECONDS));
}

TEST_F(ChannelHungTests, topologyUpdateDoesNotTriggerConnErrorCallback)
{
    rmqt::Topology emptyTopology;
    bsl::shared_ptr<ChannelTestImpl> channel =
        makeOpeningChannel(emptyTopology);

    rmqamqpt::ChannelOpenOk openOkMethod;
    channel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(openOkMethod))));

    EXPECT_THAT(channel->state(), Eq(rmqamqp::Channel::READY));

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");

    rmqt::FieldTable args;
    rmqamqpt::QueueBind queueBind("queue", "exchange", "bindKey", false, args);

    EXPECT_CALL(
        d_callback,
        onAsyncWrite(::testing::Pointee(MessageEq(rmqamqp::Message(
                         rmqamqpt::Method(rmqamqpt::QueueMethod(queueBind))))),
                     _))
        .Times(2);
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    rmqt::Future<> future = channel->updateTopology(topologyUpdate);

    // Expect that the hung timer callback isn't invoked when the BindOk
    // isn't received in time
    const int HUNG_TIMEOUT_SECONDS = 120;

    {
        EXPECT_CALL(d_callback, onHungTimerCallback()).Times(0);
        d_timerFactory->step_time(bsls::TimeInterval(HUNG_TIMEOUT_SECONDS));
    }

    // If only one BindOk comes back ensure the hung timer callback still
    // isn't called
    rmqamqpt::QueueBindOk queueBindOkMethod;
    channel->processReceived(rmqamqp::Message(
        rmqamqpt::Method(rmqamqpt::QueueMethod(queueBindOkMethod))));

    {
        EXPECT_CALL(d_callback, onHungTimerCallback()).Times(0);
        d_timerFactory->step_time(bsls::TimeInterval(HUNG_TIMEOUT_SECONDS));
    }
}
