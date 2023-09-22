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

#include <rmqamqpt_frame.h>

#include <bsl_cstring.h>
#include <bsl_stdexcept.h>

#include <bsl_vector.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqpt;

TEST(Frame, Equality)
{
    bsl::vector<uint8_t> buffer;
    const char* data = "helloworld";
    buffer.assign(data, data + 10);
    Frame f1(8, 0, buffer.data(), 10);

    EXPECT_TRUE(f1 == f1);
}

TEST(Frame, DecodeEmptyFrame)
{
    const char* emptyFrame = "\x08\x00\x00\x00\x00\x00\x00\xCE";
    bsl::vector<uint8_t> buffer(emptyFrame, emptyFrame + 8);
    bsl::size_t readBytes = 0;
    bsl::size_t remaining = 111111111;
    Frame f;
    Frame::ReturnCode decoded =
        Frame::decode(&f, &readBytes, &remaining, buffer.data(), 8);
    EXPECT_EQ(Frame::OK, decoded);
    EXPECT_EQ(remaining, 0);
}

TEST(Frame, CantFitHeartBeat)
{
    bsl::vector<uint8_t> buffer(Frame::getMaxFrameSize());
    bsl::size_t readBytes   = 0;
    bsl::size_t remaining   = 11111;
    const char* almostFrame = "\x08\x00\x00\x00\x00\x00\x00";
    buffer.assign(almostFrame, almostFrame + 7);
    Frame f1;
    Frame::ReturnCode decodable =
        Frame::decode(&f1, &readBytes, &remaining, buffer.data(), 7);

    EXPECT_EQ(Frame::PARTIAL, decodable);
}

TEST(Frame, CantFitHeartBeatBy2)
{
    bsl::vector<uint8_t> buffer(Frame::getMaxFrameSize());
    bsl::size_t readBytes   = 0;
    bsl::size_t remaining   = 11111;
    const char* almostFrame = "\x08\x00\x00\x00\x00\x00";
    buffer.assign(almostFrame, almostFrame + 6);
    Frame f1;
    Frame::ReturnCode decodable =
        Frame::decode(&f1, &readBytes, &remaining, buffer.data(), 6);

    EXPECT_EQ(Frame::PARTIAL, decodable);
}

TEST(Frame, OverSpillHeartBeat)
{
    bsl::vector<uint8_t> buffer(Frame::getMaxFrameSize());
    bsl::size_t readBytes   = 0;
    bsl::size_t remaining   = 11111;
    const char* almostFrame = "\x08\x00\x00\x00\x00\x00\x00\xCE\xFF";
    buffer.assign(almostFrame, almostFrame + 9);
    Frame f1;
    Frame::ReturnCode decodable =
        Frame::decode(&f1, &readBytes, &remaining, buffer.data(), 9);

    EXPECT_EQ(Frame::OK, decodable);
    EXPECT_EQ(f1.type(), 8);
    EXPECT_EQ(f1.channel(), 0);
    EXPECT_EQ(f1.payloadLength(), 0);
    EXPECT_EQ(remaining, 1);
    EXPECT_EQ(readBytes, 8);
}

TEST(Frame, BadSentinelChar)
{
    bsl::vector<bsl::uint8_t> buffer(Frame::getMaxFrameSize());
    bsl::size_t readBytes = 0;
    bsl::size_t remaining = 4000;

    ///< \xCE represents frame end
    const char* almostFrame = "\x08\x00\x01\x00\x00\x00\x02\xFF\xFF\xCD";
    //                                                               ^^
    buffer.assign(almostFrame, almostFrame + 1);
    Frame f1;

    EXPECT_EQ(Frame::DECODE_ERROR,
              Frame::decode(&f1, &readBytes, &remaining, buffer.data(), 10));

    EXPECT_EQ(remaining, 4000);
    EXPECT_EQ(0, readBytes);
}

TEST(Frame, OneByteTooLittle)
{
    // GIVEN
    bsl::vector<bsl::uint8_t> buffer(Frame::getMaxFrameSize());
    bsl::size_t readBytes = 0;
    bsl::size_t remaining = 4000;

    ///< \xCE represents frame end
    const char* almostFrame = "\x08\x00\x01\x00\x00\x00\x02\xFF\xFF\xCE\xFF";
    buffer.assign(almostFrame, almostFrame + 11);
    Frame f1;

    // WHEN
    Frame::ReturnCode decodable = Frame::decode(&f1,
                                                &readBytes,
                                                &remaining,
                                                buffer.data(),
                                                9); // cut the buffer short

    // THEN
    EXPECT_EQ(Frame::PARTIAL, decodable);
    EXPECT_EQ(remaining, 4000);
    EXPECT_EQ(0, readBytes);
}

TEST(Frame, OverSpillFakePayload)
{
    // GIVEN
    bsl::vector<bsl::uint8_t> buffer(Frame::getMaxFrameSize());
    bsl::size_t readBytes   = 0;
    bsl::size_t remaining   = 4000;
    const char* almostFrame = "\x08\x00\x01\x00\x00\x00\x02\xFF\xFF\xCE\xFF";
    buffer.assign(almostFrame, almostFrame + 11);
    Frame f1;

    // WHEN
    Frame::ReturnCode decodable =
        Frame::decode(&f1, &readBytes, &remaining, buffer.data(), 11);

    // THEN
    EXPECT_EQ(Frame::OK, decodable);
    EXPECT_EQ(f1.type(), 8);
    EXPECT_EQ(f1.channel(), 1);
    EXPECT_EQ(f1.payloadLength(), 2);
    EXPECT_EQ(remaining, 1);
    EXPECT_EQ(readBytes, 10);
    int payloadCmp = bsl::memcmp(f1.rawData(), almostFrame, 10);
    EXPECT_EQ(payloadCmp, 0);
}
