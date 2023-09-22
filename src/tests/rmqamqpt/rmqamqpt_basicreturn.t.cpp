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

#include <rmqamqpt_basicreturn.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_constants.h>
#include <rmqamqpt_frame.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdint.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(Methods_BasicReturn, Breathing)
{

    rmqamqpt::BasicReturn basicReturn(rmqamqpt::Constants::NO_ROUTE,
                                      "no-route",
                                      "test-exchange",
                                      "test-routing-key");
}

TEST(Methods_BasicReturn, ReturnEncodeDecode)
{
    rmqamqpt::BasicReturn basicReturn(rmqamqpt::Constants::NO_ROUTE,
                                      "no-route",
                                      "test-exchange",
                                      "test-routing-key");
    rmqamqp::Framer framer;
    uint16_t channel = 3;
    rmqamqpt::Frame frame;

    rmqamqp::Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::BasicMethod(basicReturn));

    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    EXPECT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    EXPECT_TRUE(m.is<rmqamqpt::BasicMethod>());
    const rmqamqpt::BasicMethod& bm = m.the<rmqamqpt::BasicMethod>();

    EXPECT_TRUE(bm.is<rmqamqpt::BasicReturn>());
    rmqamqpt::BasicReturn decoded = bm.the<rmqamqpt::BasicReturn>();
    EXPECT_EQ(decoded.replyCode(), rmqamqpt::Constants::NO_ROUTE);
    EXPECT_EQ(decoded.replyText(), "no-route");
    EXPECT_EQ(decoded.exchange(), "test-exchange");
    EXPECT_EQ(decoded.routingKey(), "test-routing-key");
}
