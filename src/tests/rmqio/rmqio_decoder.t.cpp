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

#include <rmqio_decoder.h>

#include <bsl_cstdint.h>
#include <bsl_cstdio.h>
#include <bsl_vector.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqio;
using namespace ::testing;

namespace {
const bsl::size_t MAX_FRAME = 100;
}

class DecoderTests : public ::testing::Test {
  protected:
    void SetUp()
    {
        const bsl::uint8_t heartbeat[] = {
            0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCE};
        d_exactFrame.insert(d_exactFrame.end(), heartbeat, &heartbeat[8]);
    }

    bsl::vector<bsl::uint8_t> d_exactFrame;
};

TEST_F(DecoderTests, ExactlyOneFrame)
{
    Decoder decoder(MAX_FRAME);
    bsl::vector<rmqamqpt::Frame> frames;

    const Decoder::ReturnCode rc =
        decoder.appendBytes(&frames, &d_exactFrame[0], d_exactFrame.size());

    EXPECT_THAT(rc, Eq(Decoder::OK));
    EXPECT_THAT(frames.size(), Eq(1));
}

TEST_F(DecoderTests, NotEnoughData)
{
    Decoder decoder(MAX_FRAME);
    bsl::vector<rmqamqpt::Frame> frames;

    EXPECT_THAT(d_exactFrame.size(), Gt(4));

    const Decoder::ReturnCode rc =
        decoder.appendBytes(&frames, &d_exactFrame[0], 4);

    EXPECT_THAT(rc, Eq(Decoder::OK));
    EXPECT_THAT(frames.size(), Eq(0));
}

TEST_F(DecoderTests, OneAndHalfFramesThenHalfFrame)
{
    Decoder decoder(MAX_FRAME);
    bsl::vector<rmqamqpt::Frame> frames;

    EXPECT_THAT(d_exactFrame.size(), Eq(8));

    // Firstly give the decoder 1.5 frames of data
    bsl::vector<bsl::uint8_t> oneAndHalfFrames(d_exactFrame.begin(),
                                               d_exactFrame.end());
    oneAndHalfFrames.insert(
        oneAndHalfFrames.end(), d_exactFrame.begin(), d_exactFrame.begin() + 4);

    const Decoder::ReturnCode rc = decoder.appendBytes(
        &frames, &oneAndHalfFrames[0], oneAndHalfFrames.size());

    EXPECT_THAT(rc, Eq(Decoder::OK));
    EXPECT_THAT(frames.size(), Eq(1));

    frames.clear();

    // Then give the remaining 0.5 frames

    const Decoder::ReturnCode rc2 =
        decoder.appendBytes(&frames, &d_exactFrame[4], 4);
    EXPECT_THAT(rc2, Eq(Decoder::OK));
    EXPECT_THAT(frames.size(), Eq(1));
}

TEST_F(DecoderTests, PartialFrameTooLarge)
{
    const bsl::size_t maxFrameLen = 4;
    Decoder decoder(maxFrameLen);
    bsl::vector<rmqamqpt::Frame> frames;

    // Pass a partial frame below maxFrameLen
    const bsl::size_t partialFrameLen = 6;
    EXPECT_THAT(d_exactFrame.size(), Gt(partialFrameLen));
    const Decoder::ReturnCode rc =
        decoder.appendBytes(&frames, &d_exactFrame[0], partialFrameLen);

    EXPECT_THAT(rc, Eq(Decoder::MAX_FRAME_SIZE));
    EXPECT_THAT(frames.size(), Eq(0));
}

TEST_F(DecoderTests, DecodeError)
{
    Decoder decoder(MAX_FRAME);
    bsl::vector<rmqamqpt::Frame> frames;

    bsl::vector<bsl::uint8_t> frameEndCorrupt = d_exactFrame;
    ASSERT_THAT(frameEndCorrupt.size(), Gt(0));
    frameEndCorrupt.back() = 0xFF; // corrupt the frame end marker

    const Decoder::ReturnCode rc = decoder.appendBytes(
        &frames, &frameEndCorrupt[0], frameEndCorrupt.size());

    EXPECT_THAT(rc, Eq(Decoder::DECODE_ERROR));
    EXPECT_THAT(frames.size(), Eq(0));
}
