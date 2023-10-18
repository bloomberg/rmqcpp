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

#include <rmqa_messageguard.h>

#include <rmqt_consumerack.h>

#include <rmqtestmocks_mockconsumer.h>

#include <bdlf_bind.h>

#include <bsl_memory.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_iostream.h>

using namespace BloombergLP;
using namespace rmqt;
using namespace ::testing;

namespace {

class MessageGuardTest : public Test {
  public:
    MessageGuardTest()
    : d_acked(false)
    , d_ack_state(rmqt::ConsumerAck::ACK)
    , d_consumer()
    {
    }
    bool d_acked;
    rmqt::ConsumerAck::Type d_ack_state;
    rmqtestmocks::MockConsumer d_consumer;

  protected:
    void ackCallback(const rmqt::ConsumerAck& ack)
    {
        EXPECT_FALSE(d_acked);
        d_acked     = true;
        d_ack_state = ack.type();
    }

    rmqa::MessageGuard::MessageGuardCallback callback()
    {
        using bdlf::PlaceHolders::_1;
        return bdlf::BindUtil::bind(&MessageGuardTest::ackCallback, this, _1);
    }
};
} // namespace

TEST_F(MessageGuardTest, Construct)
{
    rmqa::MessageGuard mg(
        rmqt::Message(),
        rmqt::Envelope(0, 0, "consumerTag", "exchange", "routing-key", false),
        callback(),
        &d_consumer);
}

TEST_F(MessageGuardTest, ackCallsCallback)
{
    rmqa::MessageGuard mg(
        rmqt::Message(),
        rmqt::Envelope(0, 0, "consumerTag", "exchange", "routing-key", false),
        callback(),
        &d_consumer);
    mg.ack();
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::ACK);
}

TEST_F(MessageGuardTest, nackRequeueCallsCallback)
{
    rmqa::MessageGuard mg(
        rmqt::Message(),
        rmqt::Envelope(0, 0, "consumerTag", "exchange", "routing-key", false),
        callback(),
        &d_consumer);
    mg.nack();
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::REQUEUE);
}

TEST_F(MessageGuardTest, nackRejectCallsCallback)
{
    rmqa::MessageGuard mg(
        rmqt::Message(),
        rmqt::Envelope(0, 0, "consumerTag", "exchange", "routing-key", false),
        callback(),
        &d_consumer);
    mg.nack(false);
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::REJECT);
}

TEST_F(MessageGuardTest, onlyFirstAckApplies)
{
    rmqa::MessageGuard mg(
        rmqt::Message(),
        rmqt::Envelope(0, 0, "consumerTag", "exchange", "routing-key", false),
        callback(),
        &d_consumer);
    mg.ack();
    mg.ack();
    mg.nack(false);
    mg.nack(true);
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::ACK);
}

TEST_F(MessageGuardTest, onlyFirstNackApplies)
{
    rmqa::MessageGuard mg(
        rmqt::Message(),
        rmqt::Envelope(0, 0, "consumerTag", "exchange", "routing-key", false),
        callback(),
        &d_consumer);
    mg.nack(true);
    mg.ack();
    mg.ack();
    mg.nack(false);
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::REQUEUE);
}

TEST_F(MessageGuardTest, destructorCallsNack)
{
    {
        rmqa::MessageGuard mg(
            rmqt::Message(),
            rmqt::Envelope(
                0, 0, "consumerTag", "exchange", "routing-key", false),
            callback(),
            &d_consumer);
    }
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::REQUEUE);
}

TEST_F(MessageGuardTest, copyingInvalidatesOriginalAndNoNackCalled)
{
    {
        rmqa::MessageGuard mg(
            rmqt::Message(),
            rmqt::Envelope(
                0, 0, "consumerTag", "exchange", "routing-key", false),
            callback(),
            &d_consumer);
        {
            rmqa::MessageGuard mg2 = mg;
            mg.ack();
            mg.nack();
            EXPECT_FALSE(d_acked);
        }
    }
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::REQUEUE);
}

TEST_F(MessageGuardTest, copiedObjectIsvalidAndCanAck)
{
    {
        rmqa::MessageGuard mg(
            rmqt::Message(),
            rmqt::Envelope(
                0, 0, "consumerTag", "exchange", "routing-key", false),
            callback(),
            &d_consumer);
        {
            rmqa::MessageGuard mg2 = mg;
            mg.ack();
            mg.nack();
            EXPECT_FALSE(d_acked);
            mg2.ack();
        }
    }
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::ACK);
}

TEST_F(MessageGuardTest, ConsumerPassThrough)
{
    rmqa::MessageGuard mg(
        rmqt::Message(),
        rmqt::Envelope(0, 0, "consumerTag", "exchange", "routing-key", false),
        callback(),
        &d_consumer);
    EXPECT_THAT(&d_consumer, Eq(mg.consumer()));
}

TEST_F(MessageGuardTest, TransferOwnership)
{
    rmqp::TransferrableMessageGuard tmg;
    {
        rmqa::MessageGuard mg(
            rmqt::Message(),
            rmqt::Envelope(
                0, 0, "consumerTag", "exchange", "routing-key", false),
            callback(),
            &d_consumer);
        tmg = mg.transferOwnership();
        EXPECT_TRUE(tmg);
        mg.ack();
        EXPECT_FALSE(d_acked);
    }
    tmg->ack();
    EXPECT_TRUE(d_acked);
    EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::ACK);
}

TEST_F(MessageGuardTest, TransferOwnershipOfAckedMG)
{
    rmqp::TransferrableMessageGuard tmg;
    {
        rmqa::MessageGuard mg(
            rmqt::Message(),
            rmqt::Envelope(
                0, 0, "consumerTag", "exchange", "routing-key", false),
            callback(),
            &d_consumer);
        mg.ack();
        EXPECT_TRUE(d_acked);
        EXPECT_EQ(d_ack_state, rmqt::ConsumerAck::ACK);
        tmg = mg.transferOwnership();
        EXPECT_TRUE(tmg);
    }
}
