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

#include <rmqa_topology.h>

#include <ball_log.h>

#include <bsl_memory.h>
#include <bsl_sstream.h>
#include <bsl_stdexcept.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqa;
using namespace ::testing;

TEST(Topology, Constructs) { rmqa::Topology t; }

TEST(Topology, AddedQueueInAreRmqtTopology)
{
    rmqa::Topology t;
    t.addQueue("test");

    rmqt::Topology tt = t.topology();

    EXPECT_THAT(tt.queues, SizeIs(1));
    EXPECT_THAT(tt.queues[0]->name(), Eq("test"));
    EXPECT_THAT(tt.exchanges, IsEmpty());
    EXPECT_THAT(tt.queueBindings, IsEmpty());
    EXPECT_THAT(tt.exchangeBindings, IsEmpty());
}

TEST(Topology, AddedExchangeAreInRmqtTopology)
{
    rmqa::Topology t;
    t.addExchange("test", rmqt::ExchangeType::DIRECT);

    rmqt::Topology tt = t.topology();

    EXPECT_THAT(tt.queues, IsEmpty());
    EXPECT_THAT(tt.exchanges, SizeIs(1));
    EXPECT_THAT(tt.exchanges[0]->name(), Eq(bsl::string("test")));
    EXPECT_THAT(tt.queueBindings, IsEmpty());
    EXPECT_THAT(tt.exchangeBindings, IsEmpty());
}

TEST(Topology, AccessingDefaultExchangeAddsItToTopology)
{
    rmqa::Topology t;
    t.defaultExchange();

    rmqt::Topology tt = t.topology();

    EXPECT_THAT(tt.queues, IsEmpty());
    EXPECT_THAT(tt.exchanges, SizeIs(1));
    EXPECT_TRUE(tt.exchanges[0]->isDefault());
    EXPECT_THAT(tt.queueBindings, IsEmpty());
    EXPECT_THAT(tt.exchangeBindings, IsEmpty());
}

TEST(Topology, AddingDuplicateQueuesReturnsEmptyPointer)
{
    rmqa::Topology t;
    t.addQueue("test");
    // Topology::addQueue returns empty pointer if name duplicated
    EXPECT_TRUE(t.addQueue("queue").lock());
    EXPECT_FALSE(t.addQueue("test").lock());
}

TEST(Topology, AddingDuplicatePassiveQueuesReturnsEmptyPointer)
{
    rmqa::Topology t;
    t.addPassiveQueue("test");
    t.addPassiveQueue("foo");
    EXPECT_TRUE(t.addQueue("queue").lock());
    EXPECT_FALSE(t.addQueue("test").lock());
    EXPECT_FALSE(t.addPassiveQueue("foo").lock());
}

TEST(Topology, AddingDuplicateExchangesReturnsEmptyPointer)
{
    rmqa::Topology t;
    t.addExchange("test");

    // Topology::addExchange returns empty pointer if name duplicated
    EXPECT_TRUE(t.addExchange("exchange").lock());
    EXPECT_FALSE(t.addExchange("test").lock());
}

TEST(Topology, AddExchangeWithIllegalNameReturnsEmptyPointer)
{
    rmqa::Topology t;

    // Topology::addExchange returns empty pointer if name is not valid AMQP
    // exchange name

    EXPECT_TRUE(t.addExchange("Invalid @name").expired());
}

TEST(Topology, AddingDuplicatePassiveExchangesReturnsEmptyPointer)
{
    rmqa::Topology t;
    t.addPassiveExchange("test");
    t.addPassiveExchange("foo");
    EXPECT_TRUE(t.addExchange("queue").lock());
    EXPECT_FALSE(t.addExchange("test").lock());
    EXPECT_FALSE(t.addPassiveExchange("foo").lock());
}

TEST(Topology, BindingQueuesAddsItToTopology)
{
    rmqa::Topology t;
    rmqt::QueueHandle q    = t.addQueue("q");
    rmqt::ExchangeHandle e = t.addExchange("e", rmqt::ExchangeType::DIRECT);
    t.bind(e, q, "k");

    rmqt::Topology tt = t.topology();

    EXPECT_THAT(tt.queues, SizeIs(1));
    EXPECT_THAT(tt.queues[0]->name(), Eq("q"));
    EXPECT_THAT(tt.exchanges, SizeIs(1));
    EXPECT_THAT(tt.exchanges[0]->name(), Eq("e"));
    EXPECT_THAT(tt.queueBindings, SizeIs(1));
    EXPECT_THAT(tt.queueBindings[0]->queue().lock(), Eq(q.lock()));
    EXPECT_THAT(tt.queueBindings[0]->exchange().lock(), Eq(e.lock()));
    EXPECT_THAT(tt.queueBindings[0]->bindingKey(), Eq("k"));
    EXPECT_THAT(tt.exchangeBindings, IsEmpty());
}

TEST(Topology, BindingQueueToPassiveExchangeWorks)
{
    rmqa::Topology t;
    rmqt::QueueHandle q    = t.addQueue("q");
    rmqt::ExchangeHandle e = t.addPassiveExchange("e");
    t.bind(e, q, "k");

    rmqt::Topology tt = t.topology();

    EXPECT_THAT(tt.queues, SizeIs(1));
    EXPECT_THAT(tt.queues[0]->name(), Eq("q"));
    EXPECT_THAT(tt.exchanges, SizeIs(1));
    EXPECT_THAT(tt.exchanges[0]->name(), Eq("e"));
    EXPECT_THAT(tt.queueBindings, SizeIs(1));
    EXPECT_THAT(tt.queueBindings[0]->queue().lock(), Eq(q.lock()));
    EXPECT_THAT(tt.queueBindings[0]->exchange().lock(), Eq(e.lock()));
    EXPECT_THAT(tt.queueBindings[0]->bindingKey(), Eq("k"));
    EXPECT_THAT(tt.exchangeBindings, IsEmpty());
}

TEST(Topology, LogStream)
{
    rmqa::Topology t;
    rmqt::QueueHandle q =
        t.addQueue("q", rmqt::AutoDelete::OFF, rmqt::Durable::ON);
    rmqt::ExchangeHandle exch = t.addExchange("exch");
    t.bind(exch, q, "binding");

    bsl::stringstream str;
    str << t;
    bsl::string result = str.str();

    EXPECT_THAT(result, HasSubstr("'q'"));
    EXPECT_THAT(result, HasSubstr("Durable"));
    EXPECT_THAT(result, HasSubstr("'exch'"));
    EXPECT_THAT(result, HasSubstr("binding"));
    EXPECT_THAT(result, Not(HasSubstr("Passive")));
    EXPECT_THAT(result, Not(HasSubstr("Auto-delete")));

    BALL_LOG_SET_CATEGORY("RMQA.TOPOLOGY.T");
    BALL_LOG_ERROR << result;
}
