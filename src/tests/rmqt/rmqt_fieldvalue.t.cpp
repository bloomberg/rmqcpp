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

#include <rmqt_fieldvalue.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_map.h>
#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace rmqt;
using namespace ::testing;

TEST(FieldArray, ExampleUsage)
{
    bsl::shared_ptr<FieldArray> array = bsl::make_shared<FieldArray>();

    array->push_back(FieldValue(10));
    array->push_back(FieldValue(true));
    array->push_back(FieldValue(bsl::string("test")));

    bsl::vector<FieldValue> fvs;
    fvs.push_back(FieldValue(10));
    fvs.push_back(FieldValue(true));
    fvs.push_back(FieldValue(bsl::string("test")));

    FieldArray array2(fvs);

    EXPECT_THAT(*array, Eq(array2));

#ifdef BSLS_COMPILERFEATURES_SUPPORT_GENERALIZED_INITIALIZERS

    FieldArray array3{
        FieldValue(10), FieldValue(true), FieldValue(bsl::string("test"))};

    bsl::shared_ptr<FieldArray> array4 = FieldArray::make(
        {FieldValue(10), FieldValue(true), FieldValue(bsl::string("test"))});

    EXPECT_THAT(*array, Eq(array3));
    EXPECT_THAT(*array, Eq(*array4));

#endif
}

TEST(FieldTable, ExampleUsage)
{
    bsl::shared_ptr<FieldTable> table = bsl::make_shared<FieldTable>();

    table->insert(
        bsl::make_pair(bsl::string("item1"), FieldValue(uint8_t(10))));
    table->insert(bsl::make_pair(bsl::string("my-bool"), FieldValue(true)));
    table->insert(bsl::make_pair(bsl::string("another-key"), FieldValue(10)));

    bsl::map<bsl::string, FieldValue> fvmap;
    fvmap.insert(bsl::make_pair(bsl::string("item1"), FieldValue(uint8_t(10))));
    fvmap.insert(bsl::make_pair(bsl::string("my-bool"), FieldValue(true)));
    fvmap.insert(bsl::make_pair(bsl::string("another-key"), FieldValue(10)));
    FieldTable table2(fvmap);

    EXPECT_THAT(*table, Eq(table2));

#ifdef BSLS_COMPILERFEATURES_SUPPORT_GENERALIZED_INITIALIZERS

    FieldTable table3{{"item1", FieldValue(uint8_t(10))},
                      {"my-bool", FieldValue(true)},
                      {"another-key", FieldValue(10)}};

    bsl::shared_ptr<FieldTable> table4 =
        FieldTable::make({{"item1", FieldValue(uint8_t(10))},
                          {"my-bool", FieldValue(true)},
                          {"another-key", FieldValue(10)}});

    EXPECT_THAT(*table, Eq(table3));
    EXPECT_THAT(*table, Eq(*table4));

#endif
}

TEST(FieldValueIntSizeTests, uint8_test)
{
    uint8_t x = 5;
    FieldValue fv(x);

    EXPECT_TRUE(fv.is<uint8_t>());
}

TEST(FieldValueIntSizeTests, int8_test)
{
    int8_t x = 5;
    FieldValue fv(x);

    EXPECT_TRUE(fv.is<int8_t>());
}

TEST(FieldValueIntSizeTests, uint16_test)
{
    uint16_t x = 5;
    FieldValue fv(x);

    EXPECT_TRUE(fv.is<uint16_t>());
}

TEST(FieldValueIntSizeTests, int16_test)
{
    int16_t x = 5;
    FieldValue fv(x);

    EXPECT_TRUE(fv.is<int16_t>());
}

TEST(FieldValueIntSizeTests, uint32_test)
{
    uint32_t x = 5;
    FieldValue fv(x);

    EXPECT_TRUE(fv.is<uint32_t>());
}

TEST(FieldValueIntSizeTests, int32_test)
{
    int32_t x = 5;
    FieldValue fv(x);

    EXPECT_TRUE(fv.is<int32_t>());
}

TEST(FieldValueIntSizeTests, uint64_test)
{
    uint64_t x = 5;
    FieldValue fv(x);

    EXPECT_TRUE(fv.is<uint64_t>());
}

TEST(FieldValueIntSizeTests, int64_test)
{
    int64_t x = 5;
    FieldValue fv(x);

    EXPECT_TRUE(fv.is<int64_t>());
}

TEST(FieldValueImplicitTests, ImplicitBool)
{
    FieldValue fv(true);

    EXPECT_TRUE(fv.is<bool>());
}
