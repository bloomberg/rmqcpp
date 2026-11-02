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

#include <rmqamqpt_basicack.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_frame.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(Methods_BasicAck, AckEncodeDecode)
{
    rmqamqpt::BasicAck basicAck(1337, true);

    rmqamqp::Framer framer;
    uint16_t channel = 1;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::BasicMethod(basicAck));

    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    EXPECT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    EXPECT_TRUE(m.is<rmqamqpt::BasicMethod>());
    const rmqamqpt::BasicMethod& bm = m.the<rmqamqpt::BasicMethod>();

    EXPECT_TRUE(bm.is<rmqamqpt::BasicAck>());
    EXPECT_EQ(bm.the<rmqamqpt::BasicAck>().deliveryTag(), 1337);
    EXPECT_EQ(bm.the<rmqamqpt::BasicAck>().multiple(), true);
}

TEST(Methods_BasicAck, TruncatedPayloadFailsToDecode)
{
    // A full Basic.Ack payload is an 8 byte delivery tag plus a flag byte
    const uint8_t full[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x39, 0x01};

    for (bsl::size_t length = 0; length < sizeof(full); ++length) {
        rmqamqpt::BasicAck ack;

        EXPECT_FALSE(rmqamqpt::BasicAck::decode(&ack, full, length))
            << "length: " << length;
    }

    rmqamqpt::BasicAck ack;
    EXPECT_TRUE(rmqamqpt::BasicAck::decode(&ack, full, sizeof(full)));
    EXPECT_EQ(ack.deliveryTag(), 1337);
    EXPECT_TRUE(ack.multiple());
}

TEST(Methods_BasicAck, MultipleIsReadFromBitZeroOnly)
{
    // multiple is bit 0 of a packed bit octet; the rest are reserved
    const uint8_t cases[][2] = {
        {0x00, 0}, {0x01, 1}, {0x02, 0}, {0x03, 1}, {0xFE, 0}, {0xFF, 1}};

    for (bsl::size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const uint8_t data[] = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x39, cases[i][0]};

        rmqamqpt::BasicAck ack;
        EXPECT_TRUE(rmqamqpt::BasicAck::decode(&ack, data, sizeof(data)));
        EXPECT_EQ(ack.deliveryTag(), 1337);
        EXPECT_EQ(static_cast<int>(ack.multiple()), cases[i][1])
            << "octet: " << static_cast<int>(cases[i][0]);
    }
}
