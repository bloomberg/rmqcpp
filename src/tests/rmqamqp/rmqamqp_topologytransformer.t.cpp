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

#include <rmqamqp_topologytransformer.h>

#include <rmqamqpt_channelopen.h>
#include <rmqamqpt_queuedeclare.h>

#include <rmqt_fieldvalue.h>
#include <rmqt_topology.h>

#include <bsl_memory.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqp;

class TopologyTransformerTests : public ::testing::Test {
  public:
    rmqt::Topology d_topology;

    void createQueue(const bsl::string& name)
    {

        d_topology.queues.push_back(bsl::make_shared<rmqt::Queue>(
            name, false, false, false, rmqt::FieldTable()));
    }

    void createExchange(const bsl::string& name)
    {
        d_topology.exchanges.push_back(bsl::make_shared<rmqt::Exchange>(name));
    }

    void assertQueueDeclare(const Message& msg)
    {
        ASSERT_TRUE(msg.is<rmqamqpt::Method>() &&
                    msg.the<rmqamqpt::Method>().is<rmqamqpt::QueueMethod>() &&
                    msg.the<rmqamqpt::Method>()
                        .the<rmqamqpt::QueueMethod>()
                        .is<rmqamqpt::QueueDeclare>());
    }

    void assertExchangeDeclare(const Message& msg)
    {
        ASSERT_TRUE(
            msg.is<rmqamqpt::Method>() &&
            msg.the<rmqamqpt::Method>().is<rmqamqpt::ExchangeMethod>() &&
            msg.the<rmqamqpt::Method>()
                .the<rmqamqpt::ExchangeMethod>()
                .is<rmqamqpt::ExchangeDeclare>());
    }

    void assertQueueBind(const Message& msg)
    {
        ASSERT_TRUE(msg.is<rmqamqpt::Method>() &&
                    msg.the<rmqamqpt::Method>().is<rmqamqpt::QueueMethod>() &&
                    msg.the<rmqamqpt::Method>()
                        .the<rmqamqpt::QueueMethod>()
                        .is<rmqamqpt::QueueBind>());
    }

    void assertQueueUnbind(const Message& msg)
    {
        ASSERT_TRUE(msg.is<rmqamqpt::Method>() &&
                    msg.the<rmqamqpt::Method>().is<rmqamqpt::QueueMethod>() &&
                    msg.the<rmqamqpt::Method>()
                        .the<rmqamqpt::QueueMethod>()
                        .is<rmqamqpt::QueueUnbind>());
    }

    void assertQueueDelete(const Message& msg)
    {
        ASSERT_TRUE(msg.is<rmqamqpt::Method>() &&
                    msg.the<rmqamqpt::Method>().is<rmqamqpt::QueueMethod>() &&
                    msg.the<rmqamqpt::Method>()
                        .the<rmqamqpt::QueueMethod>()
                        .is<rmqamqpt::QueueDelete>());
    }

    void assertExchangeBind(const Message& msg)
    {
        ASSERT_TRUE(
            msg.is<rmqamqpt::Method>() &&
            msg.the<rmqamqpt::Method>().is<rmqamqpt::ExchangeMethod>() &&
            msg.the<rmqamqpt::Method>()
                .the<rmqamqpt::ExchangeMethod>()
                .is<rmqamqpt::ExchangeBind>());
    }
};

TEST_F(TopologyTransformerTests, BreathingTest)
{
    TopologyTransformer transformer(d_topology);
    EXPECT_FALSE(transformer.hasNext());
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, SingleQueue)
{
    createQueue("test-queue");
    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    EXPECT_FALSE(transformer.hasNext());
    assertQueueDeclare(msg);

    Message reply;
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::QueueMethod(rmqamqpt::QueueDeclareOk("test-queue", 0, 0))));

    EXPECT_FALSE(transformer.isDone());
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, ServerAssignedQueueName)
{
    createQueue("");
    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    EXPECT_FALSE(transformer.hasNext());
    assertQueueDeclare(msg);

    Message reply;
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(rmqamqpt::QueueMethod(
        rmqamqpt::QueueDeclareOk("some server assigned name", 0, 0))));

    EXPECT_FALSE(transformer.isDone());
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, PassiveQueue)
{
    d_topology.queues.push_back(bsl::make_shared<rmqt::Queue>(
        "test-queue", true, false, false, rmqt::FieldTable()));

    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    EXPECT_FALSE(transformer.hasNext());
    assertQueueDeclare(msg);

    Message reply;
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::QueueMethod(rmqamqpt::QueueDeclareOk("test-queue", 0, 0))));

    EXPECT_FALSE(transformer.isDone());
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, TwoQueues)
{
    createQueue("queue1");
    createQueue("queue2");

    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg1 = transformer.getNextMessage();
    EXPECT_TRUE(transformer.hasNext());
    Message msg2 = transformer.getNextMessage();
    EXPECT_FALSE(transformer.hasNext());

    assertQueueDeclare(msg1);
    assertQueueDeclare(msg2);

    Message reply1, reply2;
    reply1.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::QueueMethod(rmqamqpt::QueueDeclareOk("queue1", 0, 0))));
    reply2.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::QueueMethod(rmqamqpt::QueueDeclareOk("queue2", 0, 0))));

    EXPECT_TRUE(transformer.processReplyMessage(reply1));
    EXPECT_FALSE(transformer.isDone());

    EXPECT_TRUE(transformer.processReplyMessage(reply2));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, WrongReply)
{
    createQueue("queue1");
    TopologyTransformer transformer(d_topology);

    Message msg = transformer.getNextMessage();
    Message reply;
    reply.assign<rmqamqpt::Method>(
        rmqamqpt::Method(rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen())));
    EXPECT_FALSE(transformer.processReplyMessage(reply));
}

TEST_F(TopologyTransformerTests, SingleExchange)
{
    createExchange("test-exchange");
    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    EXPECT_FALSE(transformer.hasNext());
    assertExchangeDeclare(msg);

    Message reply;
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::ExchangeMethod(rmqamqpt::ExchangeDeclareOk())));

    EXPECT_FALSE(transformer.isDone());
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, PassiveExchange)
{
    d_topology.exchanges.push_back(
        bsl::make_shared<rmqt::Exchange>("test-exchange", true));

    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    EXPECT_FALSE(transformer.hasNext());
    assertExchangeDeclare(msg);

    Message reply;
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::ExchangeMethod(rmqamqpt::ExchangeDeclareOk())));

    EXPECT_FALSE(transformer.isDone());
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, IgnoresDefaultExchange)
{
    createExchange("");
    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, TwoExchanges)
{
    createExchange("test-exchange-1");
    createExchange("test-exchange-2");

    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg1 = transformer.getNextMessage();
    EXPECT_TRUE(transformer.hasNext());
    Message msg2 = transformer.getNextMessage();
    EXPECT_FALSE(transformer.hasNext());

    assertExchangeDeclare(msg1);
    assertExchangeDeclare(msg2);

    Message reply1, reply2;
    reply1.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::ExchangeMethod(rmqamqpt::ExchangeDeclareOk())));
    reply2.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::ExchangeMethod(rmqamqpt::ExchangeDeclareOk())));

    EXPECT_TRUE(transformer.processReplyMessage(reply1));
    EXPECT_FALSE(transformer.isDone());

    EXPECT_TRUE(transformer.processReplyMessage(reply2));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, QueueBinding)
{
    bsl::shared_ptr<rmqt::Queue> q = bsl::make_shared<rmqt::Queue>(
        "test-queue", false, false, false, rmqt::FieldTable());
    bsl::shared_ptr<rmqt::Exchange> e =
        bsl::make_shared<rmqt::Exchange>("test-exchange");
    bsl::shared_ptr<rmqt::QueueBinding> b =
        bsl::make_shared<rmqt::QueueBinding>(e, q, "k", rmqt::FieldTable());

    d_topology.queues.push_back(q);
    d_topology.exchanges.push_back(e);
    d_topology.queueBindings.push_back(b);

    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    assertQueueDeclare(msg);

    EXPECT_TRUE(transformer.hasNext());
    msg = transformer.getNextMessage();
    assertExchangeDeclare(msg);

    EXPECT_TRUE(transformer.hasNext());
    msg = transformer.getNextMessage();
    assertQueueBind(msg);

    EXPECT_FALSE(transformer.hasNext());
    EXPECT_FALSE(transformer.isDone());

    Message reply;
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::QueueMethod(rmqamqpt::QueueDeclareOk("test-queue", 0, 0))));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_FALSE(transformer.isDone());
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::ExchangeMethod(rmqamqpt::ExchangeDeclareOk())));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_FALSE(transformer.isDone());
    reply.assign<rmqamqpt::Method>(
        rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueBindOk())));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, ExchangeBinding)
{
    bsl::shared_ptr<rmqt::Exchange> srcX =
        bsl::make_shared<rmqt::Exchange>("source-exchange");
    bsl::shared_ptr<rmqt::Exchange> desX =
        bsl::make_shared<rmqt::Exchange>("destination-exchange");
    bsl::shared_ptr<rmqt::ExchangeBinding> b =
        bsl::make_shared<rmqt::ExchangeBinding>(
            srcX, desX, "k", rmqt::FieldTable());

    d_topology.exchanges.push_back(srcX);
    d_topology.exchanges.push_back(desX);
    d_topology.exchangeBindings.push_back(b);

    TopologyTransformer transformer(d_topology);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    assertExchangeDeclare(msg);

    EXPECT_TRUE(transformer.hasNext());
    msg = transformer.getNextMessage();
    assertExchangeDeclare(msg);

    EXPECT_TRUE(transformer.hasNext());
    msg = transformer.getNextMessage();
    assertExchangeBind(msg);

    EXPECT_FALSE(transformer.hasNext());
    EXPECT_FALSE(transformer.isDone());

    Message reply;
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::ExchangeMethod(rmqamqpt::ExchangeDeclareOk())));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_FALSE(transformer.isDone());
    reply.assign<rmqamqpt::Method>(rmqamqpt::Method(
        rmqamqpt::ExchangeMethod(rmqamqpt::ExchangeDeclareOk())));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_FALSE(transformer.isDone());
    reply.assign<rmqamqpt::Method>(
        rmqamqpt::Method(rmqamqpt::ExchangeMethod(rmqamqpt::ExchangeBindOk())));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, BindingUpdateTest)
{
    bsl::shared_ptr<rmqt::Queue> q = bsl::make_shared<rmqt::Queue>(
        "test-queue", false, false, false, rmqt::FieldTable());
    bsl::shared_ptr<rmqt::Exchange> e =
        bsl::make_shared<rmqt::Exchange>("test-exchange");
    bsl::shared_ptr<rmqt::QueueBinding> b =
        bsl::make_shared<rmqt::QueueBinding>(e, q, "k", rmqt::FieldTable());
    bsl::shared_ptr<rmqt::QueueBinding> d =
        bsl::make_shared<rmqt::QueueBinding>(e, q, "d", rmqt::FieldTable());

    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(rmqt::TopologyUpdate::SupportedUpdate(b));
    topologyUpdate.updates.push_back(rmqt::TopologyUpdate::SupportedUpdate(d));

    TopologyTransformer transformer(topologyUpdate);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    assertQueueBind(msg);

    EXPECT_TRUE(transformer.hasNext());
    msg = transformer.getNextMessage();
    assertQueueBind(msg);

    EXPECT_FALSE(transformer.hasNext());
    EXPECT_FALSE(transformer.isDone());

    Message reply;
    reply.assign<rmqamqpt::Method>(
        rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueBindOk())));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_FALSE(transformer.isDone());
    reply.assign<rmqamqpt::Method>(
        rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueBindOk())));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, QueueUnbinding)
{
    bsl::shared_ptr<rmqt::Queue> q = bsl::make_shared<rmqt::Queue>(
        "test-queue", false, false, false, rmqt::FieldTable());
    bsl::shared_ptr<rmqt::Exchange> e =
        bsl::make_shared<rmqt::Exchange>("test-exchange");
    bsl::shared_ptr<rmqt::QueueUnbinding> b =
        bsl::make_shared<rmqt::QueueUnbinding>(e, q, "k", rmqt::FieldTable());

    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(rmqt::TopologyUpdate::SupportedUpdate(b));
    TopologyTransformer transformer(topologyUpdate);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    assertQueueUnbind(msg);

    EXPECT_FALSE(transformer.hasNext());
    EXPECT_FALSE(transformer.isDone());

    Message reply;
    reply.assign<rmqamqpt::Method>(
        rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueUnbindOk())));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}

TEST_F(TopologyTransformerTests, QueueDelete)
{
    bsl::shared_ptr<rmqt::QueueDelete> d =
        bsl::make_shared<rmqt::QueueDelete>("test-queue", false, false, false);

    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(rmqt::TopologyUpdate::SupportedUpdate(d));
    TopologyTransformer transformer(topologyUpdate);

    EXPECT_TRUE(transformer.hasNext());
    Message msg = transformer.getNextMessage();
    assertQueueDelete(msg);

    EXPECT_FALSE(transformer.hasNext());
    EXPECT_FALSE(transformer.isDone());

    Message reply;
    reply.assign<rmqamqpt::Method>(
        rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueDeleteOk(0))));
    EXPECT_TRUE(transformer.processReplyMessage(reply));
    EXPECT_TRUE(transformer.isDone());
}
