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

#include <rmqa_rabbitcontextimpl.h>

#include <rmqa_rabbitcontextoptions.h>
#include <rmqio_eventloop.h>
#include <rmqp_metricpublisher.h>
#include <rmqt_plaincredentials.h>
#include <rmqt_result.h>
#include <rmqt_simpleendpoint.h>
#include <rmqtestutil_mockeventloop.t.h>
#include <rmqtestutil_mockmetricpublisher.h>
#include <rmqtestutil_mocktimerfactory.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bdlmt_threadpool.h>
#include <bsl_memory.h>
#include <bslma_managedptr.h>

using namespace BloombergLP;
using namespace ::testing;
using namespace bdlf::PlaceHolders;

class RabbitContextImplTests : public Test {
  protected:
    bdlmt::ThreadPool* d_pool;
    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_mockTimerFactory;
    bsl::shared_ptr<rmqtestutil::MockEventLoop> d_mockEventLoop;
    bsl::shared_ptr<rmqio::EventLoop> d_eventLoop;
    rmqt::ErrorCallback d_onError;
    bsl::shared_ptr<rmqp::MetricPublisher> d_metricPublisher;
    rmqt::FieldTable d_clientProperties;
    bsls::TimeInterval d_messageProcessingTimeout;
    bsl::optional<bsls::TimeInterval> d_connectionErrorThreshold;
    rmqa::RabbitContextOptions d_options;

    RabbitContextImplTests()
    : d_pool(0)
    , d_mockTimerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    , d_mockEventLoop(
          bsl::make_shared<rmqtestutil::MockEventLoop>(d_mockTimerFactory))
    , d_eventLoop(d_mockEventLoop)
    , d_onError()
    , d_metricPublisher(bsl::make_shared<rmqtestutil::MockMetricPublisher>())
    , d_messageProcessingTimeout(60)
    , d_connectionErrorThreshold(bsls::TimeInterval(5 * 60))
    , d_options(rmqa::RabbitContextOptions()
                    .setErrorCallback(d_onError)
                    .setMetricPublisher(d_metricPublisher)
                    .setMessageProcessingTimeout(d_messageProcessingTimeout)
                    .setThreadpool(d_pool)
                    .setConnectionErrorThreshold(d_connectionErrorThreshold))
    {
        ON_CALL(*d_mockEventLoop, isStarted()).WillByDefault(Return(false));
        ON_CALL(*d_mockEventLoop, resolver(false))
            .WillByDefault(Return(bsl::shared_ptr<rmqio::Resolver>()));
        ON_CALL(*d_mockEventLoop, timerFactory())
            .WillByDefault(Return(bsl::shared_ptr<rmqio::TimerFactory>()));
    }

    bslma::ManagedPtr<rmqio::EventLoop> getMockEventLoop()
    {
        return d_eventLoop.managedPtr();
    }

    void createExpectations()
    {
        EXPECT_CALL(*d_mockEventLoop, resolver(false)).Times(1);
        EXPECT_CALL(*d_mockEventLoop, timerFactory())
            .Times(2)
            .WillRepeatedly(Return(d_mockTimerFactory));

        EXPECT_CALL(*d_mockEventLoop, waitForEventLoopExit(_)).Times(1);
    }
};

TEST_F(RabbitContextImplTests, ItsAlive)
{
    createExpectations();
    rmqa::RabbitContextImpl context(getMockEventLoop(), d_options);
}

TEST_F(RabbitContextImplTests, ErrorWhenProvideNullEndpoint)
{
    createExpectations();
    rmqa::RabbitContextImpl context(getMockEventLoop(), d_options);

    rmqt::Result<rmqp::Connection> conn =
        context
            .createNewConnection("name",
                                 bsl::shared_ptr<rmqt::Endpoint>(),
                                 bsl::shared_ptr<rmqt::Credentials>(),
                                 "name suffix")
            .blockResult();

    EXPECT_TRUE(!conn);
    EXPECT_EQ("Provided endpoint is NULL", conn.error());
}

TEST_F(RabbitContextImplTests, ErrorWhenProvideNullCredentials)
{
    createExpectations();
    rmqa::RabbitContextImpl context(getMockEventLoop(), d_options);

    rmqt::Result<rmqp::Connection> conn =
        context
            .createNewConnection(
                "name",
                bsl::make_shared<rmqt::SimpleEndpoint>("localhost", "/", 5672),
                bsl::shared_ptr<rmqt::Credentials>(),
                "name suffix")
            .blockResult();

    EXPECT_TRUE(!conn);
    EXPECT_EQ("Provided credentials is NULL", conn.error());
}

TEST_F(RabbitContextImplTests, ErrorWhenCantStartWorkerThread)
{
    EXPECT_CALL(*d_mockEventLoop, isStarted());

    createExpectations();
    rmqa::RabbitContextImpl context(getMockEventLoop(), d_options);

    rmqt::Result<rmqp::Connection> conn =
        context
            .createNewConnection(
                "name",
                bsl::make_shared<rmqt::SimpleEndpoint>("localhost", "/", 5672),
                bsl::make_shared<rmqt::PlainCredentials>("guest", "guest"),
                "name suffix")
            .blockResult();

    EXPECT_TRUE(!conn);
    EXPECT_EQ("Event loop worker thread not started", conn.error());
}
