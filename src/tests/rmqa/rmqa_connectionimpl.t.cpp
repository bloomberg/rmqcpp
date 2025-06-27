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

#include <rmqa_connectionimpl.h>

#include <rmqtestutil_mockchannel.t.h>
#include <rmqtestutil_mockeventloop.t.h>
#include <rmqtestutil_mockresolver.t.h>

#include <rmqa_topology.h>
#include <rmqamqp_heartbeatmanagerimpl.h>
#include <rmqio_backofflevelretrystrategy.h>
#include <rmqio_retryhandler.h>
#include <rmqt_fieldvalue.h>
#include <rmqt_future.h>
#include <rmqt_plaincredentials.h>
#include <rmqt_result.h>
#include <rmqt_simpleendpoint.h>

#include <bdlf_bind.h>
#include <bdlmt_threadpool.h>
#include <bsl_memory.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_iostream.h>
#include <rmqtestutil_mockmetricpublisher.h>
#include <rmqtestutil_mocktimerfactory.h>

using namespace BloombergLP;
using namespace rmqamqp;
using namespace rmqt;
using namespace ::testing;
using ::testing::NiceMock;

namespace {
class MockConnection : public rmqamqp::Connection {
  private:
    bsl::shared_ptr<rmqt::ConsumerAckQueue> d_ackQueue;
    bsl::shared_ptr<rmqtestutil::MockReceiveChannel> d_receiveChannel;
    bsl::shared_ptr<rmqtestutil::MockSendChannel> d_sendChannel;

  public:
    MockConnection(
        const bsl::shared_ptr<rmqio::Resolver>& resolver,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const bsl::shared_ptr<rmqamqp::HeartbeatManager>& hbManager,
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& hungTimerFactory,
        const bsl::shared_ptr<rmqamqp::ChannelFactory>& channelFactory,
        const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
        const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
        const bsl::shared_ptr<rmqt::Credentials>& credentials,
        const rmqt::FieldTable& clientProperties)
    : rmqamqp::Connection(resolver,
                          retryHandler,
                          hbManager,
                          hungTimerFactory,
                          channelFactory,
                          metricPublisher,
                          endpoint,
                          credentials,
                          clientProperties,
                          "Connection Name")
    , d_ackQueue(bsl::make_shared<rmqt::ConsumerAckQueue>())
    , d_receiveChannel(
          bsl::make_shared<rmqtestutil::MockReceiveChannel>(d_ackQueue,
                                                            retryHandler))
    , d_sendChannel(
          bsl::make_shared<rmqtestutil::MockSendChannel>(retryHandler))
    {

        ON_CALL(*this, createTopologySyncedSendChannel(_, _, _))
            .WillByDefault(Return(rmqt::Future<rmqamqp::SendChannel>(
                rmqt::Result<rmqamqp::SendChannel>(d_sendChannel))));

        ON_CALL(*this, createTopologySyncedReceiveChannel(_, _, _, _))
            .WillByDefault(Return(rmqt::Future<rmqamqp::ReceiveChannel>(
                rmqt::Result<rmqamqp::ReceiveChannel>(d_receiveChannel))));
    }

    MOCK_METHOD4(createTopologySyncedReceiveChannel,
                 rmqt::Future<rmqamqp::ReceiveChannel>(
                     const rmqt::Topology&,
                     const rmqt::ConsumerConfig&,
                     const bsl::shared_ptr<rmqio::RetryHandler>&,
                     const bsl::shared_ptr<rmqt::ConsumerAckQueue>&));

    MOCK_METHOD3(createTopologySyncedSendChannel,
                 rmqt::Future<rmqamqp::SendChannel>(
                     const rmqt::Topology& topology,
                     const bsl::shared_ptr<rmqt::Exchange>& exchange,
                     const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler));

    MOCK_METHOD1(close, void(const rmqamqp::Connection::CloseFinishCallback&));
};

class ConnectionTests : public ::testing::Test {
  public:
    bsl::shared_ptr<rmqt::Credentials> d_credentials;
    bsl::shared_ptr<rmqt::Endpoint> d_endpoint;
    bsl::shared_ptr<rmqtestutil::MockResolver> d_resolver;
    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
    NiceMock<rmqtestutil::MockEventLoop> d_eventLoop;
    bsl::shared_ptr<rmqamqp::HeartbeatManager> d_hb;
    rmqt::ErrorCallback d_onError;
    rmqt::SuccessCallback d_onSuccess;
    bsl::shared_ptr<rmqio::RetryHandler> d_retryHandler;
    bsl::shared_ptr<rmqamqp::ChannelFactory> d_channelFactory;
    bsl::shared_ptr<rmqtestutil::MockMetricPublisher> d_metricPublisher;
    bdlmt::ThreadPool d_threadPool;
    rmqa::Topology d_topology;
    rmqt::ExchangeHandle d_exchange;
    uint16_t d_maxOutstandingConfirms;
    rmqt::QueueHandle d_queue;
    rmqp::Consumer::ConsumerFunc d_onMessage;
    bsl::string d_consumerTag;
    uint16_t d_prefetchCount;
    rmqt::ConsumerConfig d_consumerConfig;
    rmqt::Tunables d_tunables;
    bsl::shared_ptr<rmqa::ConsumerImpl::Factory> d_consumerFactory;
    bsl::shared_ptr<rmqa::ProducerImpl::Factory> d_producerFactory;

    ConnectionTests()
    : d_credentials(new rmqt::PlainCredentials("", ""))
    , d_endpoint(new rmqt::SimpleEndpoint("", ""))
    , d_resolver(bsl::make_shared<rmqtestutil::MockResolver>())
    , d_timerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    , d_eventLoop(d_timerFactory)
    , d_hb(new rmqamqp::HeartbeatManagerImpl(d_timerFactory))
    , d_onError()
    , d_onSuccess()
    , d_retryHandler(bsl::make_shared<rmqio::RetryHandler>(
          d_timerFactory,
          d_onError,
          d_onSuccess,
          bsl::make_shared<rmqio::BackoffLevelRetryStrategy>()))
    , d_channelFactory(bsl::make_shared<rmqamqp::ChannelFactory>())
    , d_metricPublisher(bsl::make_shared<rmqtestutil::MockMetricPublisher>())
    , d_threadPool(bslmt::ThreadAttributes(), 0, 5, 100)
    , d_topology()
    , d_exchange(d_topology.addExchange("exchange"))
    , d_maxOutstandingConfirms(5)
    , d_queue(d_topology.addQueue("queue"))
    , d_onMessage(bdlf::BindUtil::bind(&ConnectionTests::onNewMessage,
                                       this,
                                       bdlf::PlaceHolders::_1))
    , d_consumerTag("consumertag")
    , d_prefetchCount(5)
    , d_consumerConfig(d_consumerTag, d_prefetchCount)
    , d_tunables()
    , d_consumerFactory(bsl::make_shared<rmqa::ConsumerImpl::Factory>())
    , d_producerFactory(bsl::make_shared<rmqa::ProducerImpl::Factory>())
    {
    }

    ~ConnectionTests() { d_timerFactory->cancel(); }

    MOCK_METHOD1(onNewMessage, void(const rmqp::MessageGuard& message));

    bsl::shared_ptr<MockConnection>
    createMockConnection(const bsl::string& connectionName = "my-connection")
    {
        rmqt::FieldTable clientProps;
        clientProps["connection_name"] = rmqt::FieldValue(connectionName);

        ON_CALL(*d_resolver, asyncConnect(_, _, _, _, _, _));

        bsl::shared_ptr<NiceMock<MockConnection> > mc =
            bsl::make_shared<NiceMock<MockConnection> >(d_resolver,
                                                        d_retryHandler,
                                                        d_hb,
                                                        d_timerFactory,
                                                        d_channelFactory,
                                                        d_metricPublisher,
                                                        d_endpoint,
                                                        d_credentials,
                                                        clientProps);

        ON_CALL(*mc, close(_)).WillByDefault(testing::InvokeArgument<0>());
        return mc;
    }
};
} // namespace

TEST_F(ConnectionTests, BreathingTest)
{
    bsl::shared_ptr<MockConnection> mockConn = createMockConnection();
    bsl::shared_ptr<rmqp::Connection> connectionImpl(
        rmqa::ConnectionImpl::make(mockConn,
                                   d_eventLoop,
                                   d_threadPool,
                                   d_onError,
                                   d_onSuccess,
                                   d_endpoint,
                                   d_tunables,
                                   d_consumerFactory,
                                   d_producerFactory));
}

TEST_F(ConnectionTests, CreateSyncSuccess)
{
    rmqt::Future<rmqa::ConnectionImpl>::Pair connectionImplFuture =
        rmqt::Future<rmqa::ConnectionImpl>::make();

    bsl::shared_ptr<MockConnection> mockCon = createMockConnection();

    bsl::shared_ptr<rmqa::ConnectionImpl> connection(
        rmqa::ConnectionImpl::make(mockCon,
                                   d_eventLoop,
                                   d_threadPool,
                                   d_onError,
                                   d_onSuccess,
                                   d_endpoint,
                                   d_tunables,
                                   d_consumerFactory,
                                   d_producerFactory));

    connectionImplFuture.first(rmqt::Result<rmqa::ConnectionImpl>(connection));

    bsl::shared_ptr<rmqa::ConnectionImpl> connectionImpl =
        connectionImplFuture.second.blockResult().value();
}

TEST_F(ConnectionTests, CreateSyncFail)
{
    rmqt::Future<rmqa::ConnectionImpl>::Pair connectionImplFuture =
        rmqt::Future<rmqa::ConnectionImpl>::make();

    connectionImplFuture.first(
        rmqt::Result<rmqa::ConnectionImpl>("Failed to connect"));

    rmqt::Result<rmqa::ConnectionImpl> result =
        connectionImplFuture.second.blockResult();

    bool fail = !result;

    EXPECT_THAT(fail, Eq(true));
}

TEST_F(ConnectionTests, CreateConsumer)
{
    // Given
    bsl::shared_ptr<rmqamqp::Connection> mockCon = createMockConnection();
    bsl::shared_ptr<rmqp::Connection> connection(
        rmqa::ConnectionImpl::make(mockCon,
                                   d_eventLoop,
                                   d_threadPool,
                                   d_onError,
                                   d_onSuccess,
                                   d_endpoint,
                                   d_tunables,
                                   d_consumerFactory,
                                   d_producerFactory));

    d_eventLoop.start();
    // When
    rmqt::Result<rmqp::Consumer> result = connection->createConsumer(
        d_topology.topology(), d_queue, d_onMessage, d_consumerConfig);

    // Then
    EXPECT_TRUE(result);
}

TEST_F(ConnectionTests, CreateProducer)
{
    // Given
    bsl::shared_ptr<MockConnection> mockCon = createMockConnection();
    bsl::shared_ptr<rmqp::Connection> connection(
        rmqa::ConnectionImpl::make(mockCon,
                                   d_eventLoop,
                                   d_threadPool,
                                   d_onError,
                                   d_onSuccess,
                                   d_endpoint,
                                   d_tunables,
                                   d_consumerFactory,
                                   d_producerFactory));

    // When
    rmqt::Result<rmqp::Producer> result = connection->createProducer(
        d_topology.topology(), d_exchange, d_maxOutstandingConfirms);

    // Then
    EXPECT_TRUE(result);
}

TEST_F(ConnectionTests, CloseCreatesTimerAndInvokesClose)
{
    bsl::shared_ptr<MockConnection> mockCon = createMockConnection();
    bsl::shared_ptr<rmqp::Connection> connection(
        rmqa::ConnectionImpl::make(mockCon,
                                   d_eventLoop,
                                   d_threadPool,
                                   d_onError,
                                   d_onSuccess,
                                   d_endpoint,
                                   d_tunables,
                                   d_consumerFactory,
                                   d_producerFactory));

    d_eventLoop.start();

    rmqamqp::Connection::CloseFinishCallback cancelTimerCallback;

    EXPECT_CALL(*mockCon, close(_)).WillOnce(SaveArg<0>(&cancelTimerCallback));
    EXPECT_CALL(d_eventLoop, timerFactory()).WillOnce(Return(d_timerFactory));

    connection->close();

    ASSERT_TRUE(cancelTimerCallback);
    cancelTimerCallback();

    d_timerFactory->step_time(bsls::TimeInterval(10, 0));
}

TEST_F(ConnectionTests, GracefulCloseHitsTimeoutAndSuccessfulCloseRace)
{
    bsl::shared_ptr<MockConnection> mockCon = createMockConnection();
    bsl::shared_ptr<rmqp::Connection> connection(
        rmqa::ConnectionImpl::make(mockCon,
                                   d_eventLoop,
                                   d_threadPool,
                                   d_onError,
                                   d_onSuccess,
                                   d_endpoint,
                                   d_tunables,
                                   d_consumerFactory,
                                   d_producerFactory));

    d_eventLoop.start();

    rmqamqp::Connection::CloseFinishCallback cancelTimerCallback;

    EXPECT_CALL(*mockCon, close(_)).WillOnce(SaveArg<0>(&cancelTimerCallback));
    EXPECT_CALL(d_eventLoop, timerFactory()).WillOnce(Return(d_timerFactory));

    connection->close();

    // Invoke close timoute
    d_timerFactory->step_time(bsls::TimeInterval(10, 0));

    // Also invoke the cancel time callback function to check for UB
    ASSERT_TRUE(cancelTimerCallback);
    cancelTimerCallback();
}
