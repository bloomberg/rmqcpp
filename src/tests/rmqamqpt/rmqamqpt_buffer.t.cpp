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

#include <rmqamqpt_buffer.h>

#include <bdlb_bigendian.h>

#include <bsl_cstdint.h>

#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqpt;

TEST(Buffer, Breathing)
{
    const uint8_t buf[] = {'H', 'E', 'L', 'L', 'O'};
    Buffer b(buf, sizeof(buf));
    EXPECT_EQ(b.available(), sizeof(buf));
}

TEST(Buffer, CopyByte)
{
    const uint8_t buf[] = {'H', 'E', 'L', 'L', 'O'};
    Buffer b(buf, sizeof(buf));
    EXPECT_EQ(b.available(), sizeof(buf));
    char first = 0;
    EXPECT_TRUE(b.copy<char>(&first));
    EXPECT_EQ(first, 'H');
    b.skip(3);
    char last = 0;
    EXPECT_TRUE(b.copy<char>(&last));
    EXPECT_EQ('O', last);
    EXPECT_EQ(b.available(), 0);
}

TEST(Buffer, CopyConvertsFromWireByteOrder)
{
    const uint8_t buf[] = {0x01, 0x02, 0x03, 0x04};
    Buffer b(buf, sizeof(buf));

    bsl::uint32_t value = 0;
    EXPECT_TRUE(b.copy<bdlb::BigEndianUint32>(&value));
    EXPECT_EQ(value, 0x01020304u);
    EXPECT_EQ(b.available(), 0);
}

TEST(Buffer, CopyPastEndFailsAndDoesNotAdvance)
{
    const uint8_t buf[] = {0xDE, 0xAD, 0xBE};
    Buffer b(buf, sizeof(buf));

    bsl::uint32_t tooBig = 0x5A5A5A5A;
    EXPECT_FALSE(b.copy<bdlb::BigEndianUint32>(&tooBig));

    // value and read position are both left untouched
    EXPECT_EQ(tooBig, 0x5A5A5A5Au);
    EXPECT_EQ(b.available(), sizeof(buf));

    bsl::uint8_t fits = 0;
    EXPECT_TRUE(b.copy<bsl::uint8_t>(&fits));
    EXPECT_EQ(fits, 0xDE);
    EXPECT_EQ(b.available(), 2);
}

TEST(Buffer, CopyFromEmptyBufferFails)
{
    Buffer b(NULL, 0);

    char value = 'x';
    EXPECT_FALSE(b.copy<char>(&value));
    EXPECT_EQ(value, 'x');
    EXPECT_EQ(b.available(), 0);
}

TEST(Buffer, ConsumeIsTruncatedToAvailable)
{
    const uint8_t buf[] = {'H', 'E', 'L', 'L', 'O'};
    Buffer b(buf, sizeof(buf));
    b.skip(2);

    Buffer consumed = b.consume(100);

    EXPECT_EQ(consumed.available(), 3);
    EXPECT_EQ(b.available(), 0);

    char first = 0;
    EXPECT_TRUE(consumed.copy<char>(&first));
    EXPECT_EQ('L', first);
}
