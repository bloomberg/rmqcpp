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

#include <rmqamqpt_channelflowok.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_channelmethod.h>
#include <rmqamqpt_method.h>

#include <rmqamqpt_constants.h>
#include <rmqamqpt_frame.h>

#include <bsl_cstddef.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdint.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(ChannelFlowOk, FlowOkEncodeDecode)
{
    rmqamqpt::ChannelFlowOk flowOkMethod(1);

    rmqamqp::Framer framer;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 5, rmqamqpt::ChannelMethod(flowOkMethod));

    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));
    EXPECT_EQ(channel, 5);

    EXPECT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    EXPECT_TRUE(m.is<rmqamqpt::ChannelMethod>());
    const rmqamqpt::ChannelMethod& cm = m.the<rmqamqpt::ChannelMethod>();

    EXPECT_TRUE(cm.is<rmqamqpt::ChannelFlowOk>());
    EXPECT_EQ(cm.the<rmqamqpt::ChannelFlowOk>().activeFlow(), 1);
}

TEST(Methods_ChannelFlowOk, ActiveIsReadFromBitZeroOnly)
{
    const uint8_t cases[][2] = {{0x00, 0}, {0x01, 1}, {0x02, 0}, {0xFF, 1}};

    for (bsl::size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const uint8_t data[] = {cases[i][0]};

        rmqamqpt::ChannelFlowOk flowOk;
        EXPECT_TRUE(
            rmqamqpt::ChannelFlowOk::decode(&flowOk, data, sizeof(data)));
        EXPECT_EQ(static_cast<int>(flowOk.activeFlow()), cases[i][1])
            << "octet: " << static_cast<int>(cases[i][0]);
    }
}
