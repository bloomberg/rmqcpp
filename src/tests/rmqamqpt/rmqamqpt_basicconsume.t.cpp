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

#include <rmqamqpt_basicconsume.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_frame.h>
#include <rmqt_fieldvalue.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdint.h>

using namespace BloombergLP;
using namespace ::testing;

namespace {
rmqt::FieldTable genArguments()
{
    rmqt::FieldTable result;
    int64_t testtag    = 1337;
    result["test-tag"] = rmqt::FieldValue(testtag);
    result["rmqcpp"]   = rmqt::FieldValue(bsl::string("works"));
    return result;
}
} // namespace

TEST(Methods_BasicConsume, Breathing)
{

    rmqamqpt::BasicConsume basicConsume("test_queue",
                                        "test_consumer_tag",
                                        genArguments(),
                                        false,
                                        true,
                                        false,
                                        true);
}

TEST(Methods_BasicConsume, BreathingDefaults)
{
    rmqamqpt::BasicConsume basicConsume("test_queue", "test_consumer_tag");
}

TEST(Methods_BasicConsume, ConsumeEncodeDecode)
{
    rmqamqpt::BasicConsume basicConsume("test_queue",
                                        "test_consumer_tag",
                                        genArguments(),
                                        true,
                                        true,
                                        false,
                                        false);
    rmqamqp::Framer framer;
    uint16_t channel = 3;
    rmqamqpt::Frame frame;

    rmqamqp::Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::BasicMethod(basicConsume));

    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    EXPECT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    EXPECT_TRUE(m.is<rmqamqpt::BasicMethod>());
    const rmqamqpt::BasicMethod& bm = m.the<rmqamqpt::BasicMethod>();

    EXPECT_TRUE(bm.is<rmqamqpt::BasicConsume>());
    EXPECT_EQ(bm.the<rmqamqpt::BasicConsume>().queue(), "test_queue");
    EXPECT_EQ(bm.the<rmqamqpt::BasicConsume>().consumerTag(),
              "test_consumer_tag");
    EXPECT_EQ(bm.the<rmqamqpt::BasicConsume>().arguments(), genArguments());
    EXPECT_EQ(bm.the<rmqamqpt::BasicConsume>().noLocal(), true);
    EXPECT_EQ(bm.the<rmqamqpt::BasicConsume>().exclusive(), false);
    EXPECT_EQ(bm.the<rmqamqpt::BasicConsume>().noAck(), true);
    EXPECT_EQ(bm.the<rmqamqpt::BasicConsume>().noWait(), false);
}
