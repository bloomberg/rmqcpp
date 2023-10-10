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

#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqamqpt_types.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <ball_log.h>
#include <bdlt_datetime.h>
#include <bsl_array.h>
#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_iterator.h>
#include <bsl_limits.h>
#include <bsl_memory.h>
#include <bsl_sstream.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(FieldValue, FieldValueByteArrayEncodedSize)
{
    // GIVEN
    const bsl::uint8_t byteArrayData[] = {10, 20, 30, 40, 50, 60, 70, 80};
    const bsl::vector<bsl::uint8_t> byteArray(bsl::begin(byteArrayData),
                                              bsl::end(byteArrayData));
    const rmqt::FieldValue fieldValue(byteArray);
    const bsl::size_t expectedLength =
        1 + sizeof(bsl::uint32_t) + byteArray.size();

    // THEN
    EXPECT_THAT(rmqamqpt::FieldValueUtil::encodedSize(fieldValue),
                Eq(expectedLength));

    bsl::vector<uint8_t> output;
    rmqamqpt::Writer writer(&output);

    // WHEN
    rmqamqpt::Types::encodeFieldValue(writer, fieldValue);

    // THEN
    EXPECT_THAT(output.size(), Eq(expectedLength));
    EXPECT_EQ(output[0], 'x');
    EXPECT_EQ(output[1], 0);
    EXPECT_EQ(output[2], 0);
    EXPECT_EQ(output[3], 0);
    EXPECT_EQ(output[4], 8);
    EXPECT_EQ(output[5], byteArray[0]);
    EXPECT_EQ(output[6], byteArray[1]);
    EXPECT_EQ(output[7], byteArray[2]);
    EXPECT_EQ(output[8], byteArray[3]);
    EXPECT_EQ(output[9], byteArray[4]);
    EXPECT_EQ(output[10], byteArray[5]);
    EXPECT_EQ(output[11], byteArray[6]);
    EXPECT_EQ(output[12], byteArray[7]);
}

TEST(FieldValue, FieldValueArrayEncodedSize)
{
    // GIVEN
    const rmqt::FieldValue fieldValue1(true);
    const rmqt::FieldValue fieldValue2(false);
    const rmqt::FieldValue fieldValue3(true);

    bsl::shared_ptr<rmqt::FieldArray> fieldArray =
        bsl::make_shared<rmqt::FieldArray>();
    fieldArray->push_back(fieldValue1);
    fieldArray->push_back(fieldValue2);
    fieldArray->push_back(fieldValue3);
    const rmqt::FieldValue fieldValue(fieldArray);

    const bsl::size_t expectedLength = 1 + 4 + 2 + 2 + 2;

    EXPECT_THAT(rmqamqpt::FieldValueUtil::encodedSize(fieldValue),
                Eq(expectedLength));

    bsl::vector<uint8_t> output;
    rmqamqpt::Writer writer(&output);

    // WHEN
    rmqamqpt::Types::encodeFieldValue(writer, fieldValue);

    EXPECT_THAT(output.size(), Eq(expectedLength));
}

TEST(FieldValue, FieldValueTableEncodedSize)
{
    BALL_LOG_SET_CATEGORY("RMQAMQPT.FIELDVALUE.T");
    const rmqt::FieldValue boolean(false);

    bsl::shared_ptr<rmqt::FieldTable> ft = bsl::make_shared<rmqt::FieldTable>();
    (*ft)["my-boolean"]                  = boolean;

    const rmqt::FieldValue table(ft);

    const bsl::size_t expectedLength = 1 + 4 + 11 + 2;

    EXPECT_THAT(rmqamqpt::FieldValueUtil::encodedSize(table),
                Eq(expectedLength));

    bsl::vector<uint8_t> output;
    rmqamqpt::Writer writer(&output);

    // WHEN
    BALL_LOG_TRACE << "Start encode";
    rmqamqpt::Types::encodeFieldValue(writer, table);
    BALL_LOG_TRACE << "Stop encode";

    EXPECT_THAT(output.size(), Eq(expectedLength));
}

TEST(FieldValue, FieldValueArrayWithTableEncodedSize)
{
    // GIVEN
    const rmqt::FieldValue fieldValue1(
        bdlt::Datetime(1999, 11, 5)); // party like it's 1999

    const rmqt::FieldValue boolean(false);

    bsl::shared_ptr<rmqt::FieldTable> ft = bsl::make_shared<rmqt::FieldTable>();

    (*ft)["my-boolean"] = boolean;

    const rmqt::FieldValue table(ft);

    bsl::shared_ptr<rmqt::FieldArray> fieldArray =
        bsl::make_shared<rmqt::FieldArray>();
    fieldArray->push_back(fieldValue1);
    fieldArray->push_back(table);
    const rmqt::FieldValue fieldValue(fieldArray);

    // type(field-array) + array-size + type(timestamp) + timestamp-size(8) +
    // type(field-table) + table-size + type(short-string-length) +
    // 'my-boolean'.length() + type(boolean) + bool
    const bsl::size_t expectedLength = 1 + 4 + 1 + 8 + 1 + 4 + 1 + 10 + 1 + 1;

    EXPECT_THAT(rmqamqpt::FieldValueUtil::encodedSize(fieldValue),
                Eq(expectedLength));

    bsl::vector<uint8_t> output;
    rmqamqpt::Writer writer(&output);

    // WHEN
    rmqamqpt::Types::encodeFieldValue(writer, fieldValue);

    EXPECT_THAT(output.size(), Eq(expectedLength));
}

struct FieldValueTestCase {
    rmqt::FieldValue fv;
    bsl::size_t size;
};

class FieldValuePTests : public testing::TestWithParam<FieldValueTestCase> {};

TEST_P(FieldValuePTests, EncodedSize)
{
    const FieldValueTestCase& param = GetParam();

    EXPECT_THAT(rmqamqpt::FieldValueUtil::encodedSize(param.fv),
                Eq(param.size));
}

namespace {

const FieldValueTestCase k_FV_SIZE_TEST_CASES[] = {
    {rmqt::FieldValue(bsl::string("test")), 9},
    {rmqt::FieldValue(bsl::uint8_t(2)), 2},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint8_t>::min()), 2},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint8_t>::min()), 2},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint8_t>::max()), 2},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint8_t>::max()), 2},
    {rmqt::FieldValue(bsl::int8_t(2)), 2},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int8_t>::min()), 2},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int8_t>::min()), 2},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int8_t>::max()), 2},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int8_t>::max()), 2},
    {rmqt::FieldValue(bsl::uint16_t(2)), 3},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint16_t>::min()), 3},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint16_t>::min()), 3},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint16_t>::max()), 3},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint16_t>::max()), 3},
    {rmqt::FieldValue(bsl::int16_t(2)), 3},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int16_t>::min()), 3},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int16_t>::min()), 3},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int16_t>::max()), 3},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int16_t>::max()), 3},
    {rmqt::FieldValue(bsl::uint32_t(2)), 5},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint32_t>::min()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint32_t>::min()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint32_t>::max()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint32_t>::max()), 5},
    {rmqt::FieldValue(bsl::int32_t(2)), 5},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int32_t>::min()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int32_t>::min()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int32_t>::max()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int32_t>::max()), 5},
    {rmqt::FieldValue(bsl::uint64_t(2)), 9},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint64_t>::min()), 9},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint64_t>::min()), 9},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint64_t>::max()), 9},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::uint64_t>::max()), 9},
    {rmqt::FieldValue(bsl::int64_t(2)), 9},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int64_t>::min()), 9},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int64_t>::min()), 9},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int64_t>::max()), 9},
    {rmqt::FieldValue(bsl::numeric_limits<bsl::int64_t>::max()), 9},
    {rmqt::FieldValue(float(2.0)), 5},
    {rmqt::FieldValue(double(2.0)), 9},
#if BSLS_COMPILERFEATURES_CPLUSPLUS >= 201103L
    {rmqt::FieldValue(bsl::numeric_limits<float>::lowest()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<double>::lowest()), 9},
#endif
    {rmqt::FieldValue(bsl::numeric_limits<float>::min()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<double>::min()), 9},
    {rmqt::FieldValue(bsl::numeric_limits<float>::max()), 5},
    {rmqt::FieldValue(bsl::numeric_limits<double>::max()), 9},
    {rmqt::FieldValue(false), 2},
    {rmqt::FieldValue(true), 2},
    {rmqt::FieldValue(bsl::make_shared<rmqt::FieldTable>()), 5},
    {rmqt::FieldValue(bsl::make_shared<rmqt::FieldArray>()), 5}};
} // namespace

// We need to stick to INSTANTIATE_TEST_CASE_P for a while longer
// But we do want to build with -Werror in our CI
#pragma GCC diagnostic warning "-Wdeprecated-declarations"

INSTANTIATE_TEST_CASE_P(FieldValueTests,
                        FieldValuePTests,
                        testing::ValuesIn(k_FV_SIZE_TEST_CASES));
