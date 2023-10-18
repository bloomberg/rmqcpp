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

#include <rmqamqp_channelmap.h>

#include <rmqtestutil_mockchannel.t.h>

#include <rmqamqp_channel.h>
#include <rmqamqp_sendchannel.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdint.h>
#include <bsl_memory.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(ChannelMap, GetAvailableIdWhenEmpty)
{
    rmqamqp::ChannelMap map;

    EXPECT_THAT(map.assignId(), Eq(1));
}

TEST(ChannelMap, GetAvailableIdWhenNotEmpty)
{
    rmqamqp::ChannelMap map;

    EXPECT_THAT(map.assignId(), Eq(1));
    EXPECT_THAT(map.assignId(), Eq(2));
}

TEST(ChannelMap, GetAvailableIdReuse)
{
    rmqamqp::ChannelMap map;
    EXPECT_THAT(map.assignId(), Eq(1));
    bsl::shared_ptr<rmqamqp::SendChannel> emptyChannel;

    map.associateChannel(1, emptyChannel);
    map.removeChannel(1);

    EXPECT_THAT(map.assignId(), Eq(1));
}

TEST(ChannelMap, ResetCallPlusRemoveDoesNotCall)
{
    rmqamqp::ChannelMap map;
    EXPECT_THAT(map.assignId(), Eq(1));
    bsl::shared_ptr<rmqtestutil::MockSendChannel> channel =
        bsl::make_shared<rmqtestutil::MockSendChannel>();

    map.associateChannel(1, bsl::shared_ptr<rmqamqp::SendChannel>(channel));

    EXPECT_CALL(*channel, reset(false))
        .WillOnce(Return(rmqamqp::Channel::KEEP));
    map.resetAll();

    map.removeChannel(1);

    // Do not expect call
    map.resetAll();
}

TEST(ChannelMap, ResetCleanup)
{
    rmqamqp::ChannelMap map;
    EXPECT_THAT(map.assignId(), Eq(1));
    bsl::shared_ptr<rmqtestutil::MockSendChannel> channel =
        bsl::make_shared<rmqtestutil::MockSendChannel>();
    map.associateChannel(1, bsl::shared_ptr<rmqamqp::SendChannel>(channel));

    EXPECT_THAT(channel.use_count(), Gt(1));

    EXPECT_CALL(*channel, reset(_)).WillOnce(Return(rmqamqp::Channel::CLEANUP));

    map.resetAll();

    EXPECT_THAT(channel.use_count(), Eq(1));
}

TEST(ChannelMap, OpenCall)
{
    rmqamqp::ChannelMap map;
    EXPECT_THAT(map.assignId(), Eq(1));
    bsl::shared_ptr<rmqtestutil::MockSendChannel> channel =
        bsl::make_shared<rmqtestutil::MockSendChannel>();

    map.associateChannel(1, bsl::shared_ptr<rmqamqp::SendChannel>(channel));

    EXPECT_CALL(*channel, open());

    map.openAll();
}

TEST(ChannelMap, ProcessReceivedWithChannel)
{
    rmqamqp::ChannelMap map;
    EXPECT_THAT(map.assignId(), Eq(1));
    bsl::shared_ptr<rmqtestutil::MockSendChannel> channel =
        bsl::make_shared<rmqtestutil::MockSendChannel>();
    map.associateChannel(1, bsl::shared_ptr<rmqamqp::SendChannel>(channel));

    EXPECT_CALL(*channel, processReceived(_))
        .WillOnce(Return(rmqamqp::Channel::KEEP));

    rmqamqp::Message msg;
    EXPECT_TRUE(map.processReceived(1, msg));
}

TEST(ChannelMap, ProcessReceivedWithStaleChannel)
{
    rmqamqp::ChannelMap map;

    rmqamqp::Message msg;
    EXPECT_FALSE(map.processReceived(1, msg));
}

TEST(ChannelMap, ProcessReceivedCleanup)
{
    rmqamqp::ChannelMap map;
    EXPECT_THAT(map.assignId(), Eq(1));
    bsl::shared_ptr<rmqtestutil::MockSendChannel> channel =
        bsl::make_shared<rmqtestutil::MockSendChannel>();
    map.associateChannel(1, bsl::shared_ptr<rmqamqp::SendChannel>(channel));

    EXPECT_THAT(channel.use_count(), Gt(1));

    EXPECT_CALL(*channel, processReceived(_))
        .WillOnce(Return(rmqamqp::Channel::CLEANUP));

    rmqamqp::Message msg;
    EXPECT_TRUE(map.processReceived(1, msg));

    EXPECT_THAT(channel.use_count(), Eq(1));
}

TEST(ChannelMap, RemoveChannel)
{
    rmqamqp::ChannelMap map;
    EXPECT_THAT(map.assignId(), Eq(1));
    bsl::shared_ptr<rmqtestutil::MockSendChannel> channel =
        bsl::make_shared<rmqtestutil::MockSendChannel>();
    map.associateChannel(1, bsl::shared_ptr<rmqamqp::SendChannel>(channel));

    EXPECT_THAT(channel.use_count(), Gt(1));

    map.removeChannel(1);

    EXPECT_THAT(channel.use_count(), Eq(1));
}

TEST(ChannelMap, RemoveStaleChannelDoesNothing)
{
    rmqamqp::ChannelMap map;

    map.removeChannel(1);
}

TEST(ChannelMap, GetSendChannels)
{
    rmqamqp::ChannelMap map;
    bsl::shared_ptr<rmqtestutil::MockSendChannel> channel =
        bsl::make_shared<rmqtestutil::MockSendChannel>();
    rmqamqp::ChannelMap::SendChannelMap m;
    m[1] = channel;
    map.associateChannel(1, bsl::shared_ptr<rmqamqp::SendChannel>(channel));
    EXPECT_EQ(map.getSendChannels(), m);
}

TEST(ChannelMap, GetReceiveChannels)
{
    rmqamqp::ChannelMap map;
    bsl::shared_ptr<rmqtestutil::MockReceiveChannel> channel =
        bsl::make_shared<rmqtestutil::MockReceiveChannel>(
            bsl::make_shared<rmqt::ConsumerAckQueue>());
    rmqamqp::ChannelMap::ReceiveChannelMap m;
    m[1] = channel;
    map.associateChannel(1, bsl::shared_ptr<rmqamqp::ReceiveChannel>(channel));
    EXPECT_EQ(map.getReceiveChannels(), m);
}
