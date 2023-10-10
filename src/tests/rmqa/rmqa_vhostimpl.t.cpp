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

#include <rmqa_consumer.h>
#include <rmqa_producer.h>
#include <rmqa_topology.h>
#include <rmqa_vhostimpl.h>

#include <rmqt_future.h>
#include <rmqt_simpleendpoint.h>

#include <rmqtestmocks_mockconnection.h>
#include <rmqtestmocks_mockconsumer.h>
#include <rmqtestmocks_mockproducer.h>

#include <bdlf_bind.h>
#include <bsl_memory.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_iostream.h>

using namespace BloombergLP;
using namespace ::testing;

namespace {

class MockConnectionMaker {
  public:
    MockConnectionMaker()
    {
        ON_CALL(*this, create(bsl::string("consumer")))
            .WillByDefault(Return(consumerConnection.success()));
        ON_CALL(*this, create(bsl::string("producer")))
            .WillByDefault(Return(producerConnection.success()));
    }
    MOCK_METHOD1(create, rmqt::Future<rmqp::Connection>(const bsl::string&));

    rmqtestmocks::MockConnection producerConnection;
    rmqtestmocks::MockConnection consumerConnection;
    rmqtestmocks::MockProducer producer;
    rmqtestmocks::MockConsumer consumer;
}; // namespace

class VHostTests : public ::testing::Test {
  public:
    MockConnectionMaker connectionMaker;
    rmqa::VHostImpl::ConnectionMaker connectionMakerFunc;
    rmqa::Topology topology;
    rmqt::ExchangeHandle exchange;
    uint16_t maxOutstandingConfirms;
    rmqt::QueueHandle queue;
    rmqp::Consumer::ConsumerFunc onMessage;
    bsl::string consumerTag;
    uint16_t prefetchCount;
    rmqt::ConsumerConfig consumerConfig;

    VHostTests()
    : connectionMaker()
    , connectionMakerFunc(bdlf::BindUtil::bind(&MockConnectionMaker::create,
                                               &connectionMaker,
                                               bdlf::PlaceHolders::_1))
    , topology()
    , exchange(topology.addExchange("exchange"))
    , maxOutstandingConfirms(5)
    , queue(topology.addQueue("queue"))
    , onMessage(bdlf::BindUtil::bind(&VHostTests::onNewMessage,
                                     this,
                                     bdlf::PlaceHolders::_1))
    , consumerTag("consumertag")
    , prefetchCount(5)
    , consumerConfig(consumerTag, prefetchCount)
    {
    }
    MOCK_METHOD1(onNewMessage, void(rmqp::MessageGuard&));

    rmqt::Future<rmqp::Producer> newProducer(rmqa::VHostImpl& vhost)
    {

        EXPECT_CALL(connectionMaker, create(HasSubstr("producer"))).Times(1);
        EXPECT_CALL(connectionMaker.producerConnection,
                    createProducerAsync(_, _, _))
            .WillOnce(Return(connectionMaker.producer.successAsync()));
        return vhost.createProducerAsync(
            topology.topology(), exchange, maxOutstandingConfirms);
    }
    rmqt::Future<rmqp::Consumer> newConsumer(rmqa::VHostImpl& vhost)
    {

        EXPECT_CALL(connectionMaker, create(HasSubstr("consumer"))).Times(1);
        EXPECT_CALL(connectionMaker.consumerConnection,
                    createConsumerAsync(_, _, _, _))
            .WillOnce(Return(connectionMaker.consumer.successAsync()));
        return vhost.createConsumerAsync(
            topology.topology(), queue, onMessage, consumerConfig);
    }
};

} // namespace

TEST_F(VHostTests, BreathingTest)
{
    rmqa::VHostImpl vhostImpl(connectionMakerFunc);
}

TEST_F(VHostTests, CreateProducerConsumer)
{
    rmqa::VHostImpl vhostImpl(connectionMakerFunc);
    rmqt::Future<rmqp::Producer> producer = newProducer(vhostImpl);
    EXPECT_TRUE(producer.blockResult());

    rmqt::Future<rmqp::Consumer> consumer = newConsumer(vhostImpl);
    EXPECT_TRUE(consumer.blockResult());
}

TEST_F(VHostTests, closeNormalCase)
{
    rmqa::VHostImpl vhostImpl(connectionMakerFunc);
    {
        rmqt::Future<rmqp::Producer> producer = newProducer(vhostImpl);
        EXPECT_TRUE(producer.blockResult());

        rmqt::Future<rmqp::Consumer> consumer = newConsumer(vhostImpl);
        EXPECT_TRUE(consumer.blockResult());
    }
    EXPECT_CALL(connectionMaker.producerConnection, close()).Times(1);

    EXPECT_CALL(connectionMaker.consumerConnection, close()).Times(1);

    vhostImpl.close();
}
