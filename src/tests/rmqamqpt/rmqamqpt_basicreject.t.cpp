// Copyright 2026 Bloomberg Finance L.P.
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

#include <rmqamqpt_basicreject.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_frame.h>
#include <rmqamqpt_writer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdint.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace ::testing;

namespace {

rmqamqpt::BasicReject roundTrip(const rmqamqpt::BasicReject& sent)
{
    rmqamqp::Framer framer;
    uint16_t channel = 1;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::BasicMethod(sent));

    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    EXPECT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    EXPECT_TRUE(m.is<rmqamqpt::BasicMethod>());
    const rmqamqpt::BasicMethod& bm = m.the<rmqamqpt::BasicMethod>();

    EXPECT_TRUE(bm.is<rmqamqpt::BasicReject>());
    return bm.the<rmqamqpt::BasicReject>();
}

bsl::vector<uint8_t> encode(const rmqamqpt::BasicReject& reject)
{
    bsl::vector<uint8_t> storage;
    rmqamqpt::Writer writer(&storage);
    rmqamqpt::BasicReject::encode(writer, reject);
    return storage;
}

} // namespace

TEST(Methods_BasicReject, RejectRequeueEncodeDecode)
{
    const rmqamqpt::BasicReject decoded =
        roundTrip(rmqamqpt::BasicReject(1337, true));

    EXPECT_EQ(decoded.deliveryTag(), 1337);
    EXPECT_EQ(decoded.requeue(), true);
}

TEST(Methods_BasicReject, RejectNoRequeueEncodeDecode)
{
    const rmqamqpt::BasicReject decoded =
        roundTrip(rmqamqpt::BasicReject(42, false));

    EXPECT_EQ(decoded.deliveryTag(), 42);
    EXPECT_EQ(decoded.requeue(), false);
}

TEST(Methods_BasicReject, DefaultConstructedRoundTrips)
{
    const rmqamqpt::BasicReject decoded = roundTrip(rmqamqpt::BasicReject());

    EXPECT_EQ(decoded.deliveryTag(), 0);
    EXPECT_EQ(decoded.requeue(), false);
}

TEST(Methods_BasicReject, EncodedSize)
{
    EXPECT_EQ(rmqamqpt::BasicReject(1337, true).encodedSize(), 9);
    EXPECT_EQ(encode(rmqamqpt::BasicReject(1337, true)).size(), 9);
}

TEST(Methods_BasicReject, ExactWireFormat)
{
    // Basic.Reject carries a big-endian 64 bit delivery-tag followed by a
    // single bitmask octet whose bit 0 is `requeue`. Basic.Nack has
    // `multiple` in that position instead, so pin the bytes here to stop the
    // two drifting together.
    const bsl::vector<uint8_t> requeued =
        encode(rmqamqpt::BasicReject(1, true));

    ASSERT_EQ(requeued.size(), 9u);
    EXPECT_THAT(requeued,
                ElementsAre(0x00,
                            0x00,
                            0x00,
                            0x00,
                            0x00,
                            0x00,
                            0x00,
                            0x01, // delivery-tag = 1
                            0x01  // requeue = true  (bit 0)
                            ));

    const bsl::vector<uint8_t> discarded =
        encode(rmqamqpt::BasicReject(1, false));

    ASSERT_EQ(discarded.size(), 9u);
    EXPECT_THAT(discarded,
                ElementsAre(0x00,
                            0x00,
                            0x00,
                            0x00,
                            0x00,
                            0x00,
                            0x00,
                            0x01, // delivery-tag = 1
                            0x00  // requeue = false
                            ));
}

TEST(Methods_BasicReject, DecodeRejectsTruncatedPayload)
{
    // delivery-tag present, bitmask octet missing
    const uint8_t truncated[] = {0, 0, 0, 0, 0, 0, 0, 1};

    rmqamqpt::BasicReject reject;
    EXPECT_FALSE(
        rmqamqpt::BasicReject::decode(&reject, truncated, sizeof(truncated)));
}

TEST(Methods_BasicReject, DecodeReadsRequeueFromBitZeroOnly)
{
    // Basic.Nack puts `multiple` at bit 0 and `requeue` at bit 1.
    // Basic.Reject has no `multiple`: bit 0 is `requeue`, and nothing else
    // in the octet may be read as one.
    const uint8_t bitOneSet[] = {0, 0, 0, 0, 0, 0, 0, 7, 0x02};

    rmqamqpt::BasicReject reject;
    ASSERT_TRUE(
        rmqamqpt::BasicReject::decode(&reject, bitOneSet, sizeof(bitOneSet)));
    EXPECT_EQ(reject.deliveryTag(), 7u);
    EXPECT_FALSE(reject.requeue());

    const uint8_t bitZeroSet[] = {0, 0, 0, 0, 0, 0, 0, 7, 0x01};

    ASSERT_TRUE(
        rmqamqpt::BasicReject::decode(&reject, bitZeroSet, sizeof(bitZeroSet)));
    EXPECT_TRUE(reject.requeue());
}
