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

#include <rmqamqpt_connectionclose.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_connectionmethod.h>
#include <rmqamqpt_method.h>

#include <rmqamqpt_constants.h>
#include <rmqamqpt_frame.h>

#include <bsl_cstdint.h>
#include <bsl_vector.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqpt;
using namespace rmqamqp;
using namespace ::testing;

TEST(ConnectionClose, CloseEncodeDecode)
{
    ConnectionClose closeMethod(Constants::REPLY_SUCCESS, "OK");

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 0, rmqamqpt::ConnectionMethod(closeMethod));

    rmqamqp::Framer framer;
    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    ASSERT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    ASSERT_TRUE(m.is<rmqamqpt::ConnectionMethod>());
    const rmqamqpt::ConnectionMethod& cm = m.the<rmqamqpt::ConnectionMethod>();

    ASSERT_TRUE(cm.is<rmqamqpt::ConnectionClose>());

    const rmqamqpt::ConnectionClose& decodedClose =
        cm.the<rmqamqpt::ConnectionClose>();

    EXPECT_EQ(decodedClose.replyCode(), Constants::REPLY_SUCCESS);
    EXPECT_EQ(decodedClose.replyText(), "OK");
    EXPECT_EQ(decodedClose.classId(), Constants::NO_CLASS);
    EXPECT_EQ(decodedClose.methodId(), Constants::NO_METHOD);
}

TEST(ConnectionClose, CloseWithErrorEncodeDecode)
{
    ConnectionClose closeMethod(Constants::FRAME_ERROR,
                                "Invalid frame",
                                Constants::CONNECTION,
                                Constants::CONNECTION_TUNE);

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 0, rmqamqpt::ConnectionMethod(closeMethod));

    rmqamqp::Framer framer;
    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    ASSERT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    ASSERT_TRUE(m.is<rmqamqpt::ConnectionMethod>());
    const rmqamqpt::ConnectionMethod& cm = m.the<rmqamqpt::ConnectionMethod>();

    ASSERT_TRUE(cm.is<rmqamqpt::ConnectionClose>());

    const rmqamqpt::ConnectionClose& decodedClose =
        cm.the<rmqamqpt::ConnectionClose>();

    EXPECT_EQ(decodedClose.replyCode(), Constants::FRAME_ERROR);
    EXPECT_EQ(decodedClose.replyText(), "Invalid frame");
    EXPECT_EQ(decodedClose.classId(), Constants::CONNECTION);
    EXPECT_EQ(decodedClose.methodId(), Constants::CONNECTION_TUNE);
}
