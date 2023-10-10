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

#include <rmqtestutil_replayframe.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_connectionmethod.h>
#include <rmqamqpt_connectionopen.h>
#include <rmqamqpt_connectionopenok.h>

#include <bsl_memory.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqtestutil;
using namespace ::testing;

TEST(ReplayFrame, UsingEmptyConstructor)
{
    ReplayFrame rf;
    EXPECT_EQ(rf.getLength(), 0);

    rmqamqpt::ConnectionOpen open;
    rmqamqpt::ConnectionOpenOk openOk;
    rmqamqpt::Frame openFrame, openOkFrame;
    rmqamqp::Framer::makeMethodFrame(
        &openFrame, 0, rmqamqpt::ConnectionMethod(open));
    rf.pushOutbound(bsl::make_shared<rmqio::SerializedFrame>(openFrame));
    rmqamqp::Framer::makeMethodFrame(
        &openOkFrame, 0, rmqamqpt::ConnectionMethod(openOk));
    rf.pushInbound(bsl::make_shared<rmqio::SerializedFrame>(openOkFrame));

    rmqio::SerializedFrame serializedOpen(openFrame);
    rmqio::SerializedFrame serializedOpenOk(openOkFrame);

    EXPECT_THAT(rf.getLength(), Eq(2));
    EXPECT_TRUE(serializedOpen == *rf.writeFrame());
    EXPECT_TRUE(serializedOpenOk == *rf.getFrame());
    EXPECT_THAT(rf.getLength(), Eq(0));
}

TEST(ReplayFrame, UsingParameterizedConstructor)
{
    rmqamqpt::ConnectionOpen open;
    rmqamqpt::ConnectionOpenOk openOk;
    rmqamqpt::Frame openFrame, openOkFrame;
    ReplayFrame::ReplayDeque deque;
    rmqamqp::Framer::makeMethodFrame(
        &openFrame, 0, rmqamqpt::ConnectionMethod(open));
    deque.push_back(
        bsl::make_pair(bsl::make_shared<rmqio::SerializedFrame>(openFrame),
                       ReplayFrame::OUTBOUND));
    rmqamqp::Framer::makeMethodFrame(
        &openOkFrame, 0, rmqamqpt::ConnectionMethod(openOk));
    deque.push_back(
        bsl::make_pair(bsl::make_shared<rmqio::SerializedFrame>(openOkFrame),
                       ReplayFrame::INBOUND));

    rmqio::SerializedFrame serializedOpen(openFrame);
    rmqio::SerializedFrame serializedOpenOk(openOkFrame);

    ReplayFrame rf(deque);
    EXPECT_THAT(rf.getLength(), Eq(2));
    EXPECT_TRUE(serializedOpen == *rf.writeFrame());
    EXPECT_TRUE(serializedOpenOk == *rf.getFrame());
    EXPECT_THAT(rf.getLength(), Eq(0));
}

TEST(ReplayFrame, ExpectingInboundFrame)
{
    ReplayFrame rf;
    rmqamqpt::ConnectionOpen open;
    rmqamqpt::Frame openFrame;
    rmqamqp::Framer::makeMethodFrame(
        &openFrame, 0, rmqamqpt::ConnectionMethod(open));
    rf.pushInbound(bsl::make_shared<rmqio::SerializedFrame>(openFrame));

    EXPECT_THAT(rf.getLength(), Eq(1));
    EXPECT_ANY_THROW(rf.writeFrame());
}

TEST(ReplayFrame, ExpectingOutboundFrame)
{
    ReplayFrame rf;
    rmqamqpt::ConnectionOpenOk openOk;
    rmqamqpt::Frame openOkFrame;
    rmqamqp::Framer::makeMethodFrame(
        &openOkFrame, 0, rmqamqpt::ConnectionMethod(openOk));
    rf.pushOutbound(bsl::make_shared<rmqio::SerializedFrame>(openOkFrame));

    EXPECT_THAT(rf.getLength(), Eq(1));
    EXPECT_ANY_THROW(rf.getFrame());
}
