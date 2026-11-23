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

#include <rmqamqpt_basicqos.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_frame.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(Methods_BasicQoS, QoSEncodeDecode)
{
    rmqamqpt::BasicQoS basicQoS(1337, 0xb19, true);

    rmqamqp::Framer framer;
    uint16_t channel = 9;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::BasicMethod(basicQoS));

    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    EXPECT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    EXPECT_TRUE(m.is<rmqamqpt::BasicMethod>());
    const rmqamqpt::BasicMethod& bm = m.the<rmqamqpt::BasicMethod>();

    EXPECT_TRUE(bm.is<rmqamqpt::BasicQoS>());
    EXPECT_EQ(bm.the<rmqamqpt::BasicQoS>().prefetchCount(), 1337);
    EXPECT_EQ(bm.the<rmqamqpt::BasicQoS>().prefetchSize(), 0xb19);
    EXPECT_EQ(bm.the<rmqamqpt::BasicQoS>().global(), true);
}

TEST(Methods_BasicQoS, GlobalIsReadFromBitZeroOnly)
{
    const uint8_t cases[][2] = {{0x00, 0}, {0x01, 1}, {0x02, 0}, {0xFF, 1}};

    for (bsl::size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const uint8_t data[] = {
            0x00, 0x00, 0x00, 0x0A, 0x00, 0x14, cases[i][0]};

        rmqamqpt::BasicQoS qos;
        EXPECT_TRUE(rmqamqpt::BasicQoS::decode(&qos, data, sizeof(data)));
        EXPECT_EQ(qos.prefetchSize(), 10);
        EXPECT_EQ(qos.prefetchCount(), 20);
        EXPECT_EQ(static_cast<int>(qos.global()), cases[i][1])
            << "octet: " << static_cast<int>(cases[i][0]);
    }
}

TEST(Methods_BasicQoS, TruncatedPayloadFailsToDecode)
{
    const uint8_t full[] = {0x00, 0x00, 0x00, 0x0A, 0x00, 0x14, 0x01};

    for (bsl::size_t length = 0; length < sizeof(full); ++length) {
        rmqamqpt::BasicQoS qos;
        EXPECT_FALSE(rmqamqpt::BasicQoS::decode(&qos, full, length))
            << "length: " << length;
    }
}
