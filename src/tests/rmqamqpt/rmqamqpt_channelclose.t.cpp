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

#include <rmqamqpt_channelclose.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_channelmethod.h>
#include <rmqamqpt_method.h>

#include <rmqamqpt_constants.h>
#include <rmqamqpt_frame.h>

#include <bsl_cstdint.h>
#include <bsl_vector.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(ChannelClose, CloseEncodeDecode)
{
    rmqamqpt::ChannelClose closeMethod(rmqamqpt::Constants::REPLY_SUCCESS,
                                       "OK");

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 5, rmqamqpt::ChannelMethod(closeMethod));

    rmqamqp::Framer framer;
    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));
    EXPECT_EQ(channel, 5);

    ASSERT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    ASSERT_TRUE(m.is<rmqamqpt::ChannelMethod>());
    const rmqamqpt::ChannelMethod& cm = m.the<rmqamqpt::ChannelMethod>();

    ASSERT_TRUE(cm.is<rmqamqpt::ChannelClose>());

    const rmqamqpt::ChannelClose& decodedClose =
        cm.the<rmqamqpt::ChannelClose>();

    EXPECT_EQ(decodedClose.replyCode(), rmqamqpt::Constants::REPLY_SUCCESS);
    EXPECT_EQ(decodedClose.replyText(), "OK");
    EXPECT_EQ(decodedClose.classId(), rmqamqpt::Constants::NO_CLASS);
    EXPECT_EQ(decodedClose.methodId(), rmqamqpt::Constants::NO_METHOD);
}

TEST(ChannelClose, CloseWithErrorEncodeDecode)
{
    rmqamqpt::ChannelClose closeMethod(rmqamqpt::Constants::FRAME_ERROR,
                                       "Invalid frame",
                                       rmqamqpt::Constants::CHANNEL,
                                       rmqamqpt::Constants::CHANNEL_FLOW);

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 5, rmqamqpt::ChannelMethod(closeMethod));

    rmqamqp::Framer framer;
    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));
    EXPECT_EQ(channel, 5);

    ASSERT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    ASSERT_TRUE(m.is<rmqamqpt::ChannelMethod>());
    const rmqamqpt::ChannelMethod& cm = m.the<rmqamqpt::ChannelMethod>();

    ASSERT_TRUE(cm.is<rmqamqpt::ChannelClose>());

    const rmqamqpt::ChannelClose& decodedClose =
        cm.the<rmqamqpt::ChannelClose>();

    EXPECT_EQ(decodedClose.replyCode(), rmqamqpt::Constants::FRAME_ERROR);
    EXPECT_EQ(decodedClose.replyText(), "Invalid frame");
    EXPECT_EQ(decodedClose.classId(), rmqamqpt::Constants::CHANNEL);
    EXPECT_EQ(decodedClose.methodId(), rmqamqpt::Constants::CHANNEL_FLOW);
}
