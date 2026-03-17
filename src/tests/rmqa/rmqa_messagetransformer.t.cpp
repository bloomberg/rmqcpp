// Copyright 2025 Bloomberg Finance L.P.
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

#include <rmqa_compressiontransformer.h>
#include <rmqp_messagetransformer.h>
#include <rmqt_fieldvalue.h>
#include <rmqt_message.h>
#include <rmqt_result.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqa;
using namespace ::testing;

namespace {
class TransformerTester {
  public:
    enum ExpectedResult { SUCCESS, IGNORED, FAILURE };

    static void test(rmqp::MessageTransformer& transformer,
                     const rmqt::Message& message,
                     ExpectedResult expectedResult)
    {
        bsl::shared_ptr<bsl::vector<uint8_t> > data =
            bsl::make_shared<bsl::vector<uint8_t> >(
                message.payload(), message.payload() + message.payloadSize());
        rmqt::Properties props = message.properties();
        if (!props.headers) {
            props.headers = bsl::make_shared<rmqt::FieldTable>();
        }
        rmqt::Result<bool> result = transformer.transform(data, props);
        if (expectedResult == FAILURE) {
            EXPECT_FALSE(result); // Expect failure
            return;
        }
        ASSERT_TRUE(result); // Expect success or ignored
        if (expectedResult == IGNORED) {
            EXPECT_FALSE(*result.value()); // Task ignored
            return;
        }
        ASSERT_TRUE(*result.value()); // Task succeeded

        rmqt::Result<> inverseResult =
            transformer.inverseTransform(data, props);
        ASSERT_TRUE(inverseResult);

        bsl::vector<uint8_t> originalData(
            message.payload(), message.payload() + message.payloadSize());
        EXPECT_EQ(*data, originalData);
        if (!message.properties().headers) {
            // original headers empty --> new headers should be empty too
            EXPECT_TRUE(!props.headers || props.headers->empty());
        }
        else {
            // otherwise they better exist
            ASSERT_TRUE(props.headers);
            EXPECT_EQ(props, message.properties());
        }
    }
};

// Basic transform that reverse the payload to test for message integrity
// and correctness of the transformation.
class BasicMessageTransform : public rmqp::MessageTransformer {
  public:
    rmqt::Result<bool> transform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                                 rmqt::Properties&) BSLS_KEYWORD_OVERRIDE
    {
        for (size_t i = 0; i < data->size() / 2; ++i) {
            uint8_t tmp                   = (*data)[i];
            (*data)[i]                    = (*data)[data->size() - 1 - i];
            (*data)[data->size() - 1 - i] = tmp;
        }
        return rmqt::Result<bool>(bsl::make_shared<bool>(true));
    }

    rmqt::Result<>
    inverseTransform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                     rmqt::Properties& props) BSLS_KEYWORD_OVERRIDE
    {
        transform(data, props);
        return rmqt::Result<>();
    }

    bsl::string name() const BSLS_KEYWORD_OVERRIDE
    {
        return "BasicMessageTransform";
    }
};
} // namespace

TEST(MessageBuilderTests, BasicTransformIsValid)
{
    // Build the message
    bsl::string s = "[1, 2, 3]";
    rmqt::Message message(
        bsl::make_shared<bsl::vector<uint8_t> >(s.cbegin(), s.cend()));

    // Test
    BasicMessageTransform transformer;
    TransformerTester::test(transformer, message, TransformerTester::SUCCESS);
}

#ifdef RMQCPP_ENABLE_COMPRESSION
TEST(MessageBuilderTests, CompressionTransformIsValid)
{
    // Build the messages
    bsl::string s1 = "[1, 2, 3]";
    rmqt::Message msg1(
        bsl::make_shared<bsl::vector<uint8_t> >(s1.cbegin(), s1.cend()));

    bsl::string s2 = "lorem ipsum dolor sit amet" + bsl::string(16 * 1024, 'x');
    rmqt::Message msg2(
        bsl::make_shared<bsl::vector<uint8_t> >(s2.cbegin(), s2.cend()));

    rmqt::Result<rmqp::MessageTransformer> transformer =
        rmqa::CompressionTransformer::create();

    // Small messages should not be compressed
    TransformerTester::test(
        *transformer.value(), msg1, TransformerTester::IGNORED);

    // While larger ones should
    TransformerTester::test(
        *transformer.value(), msg2, TransformerTester::SUCCESS);
}

TEST(MessageBuilderTests, ChainedCompressionIsValid)
{
    // Setup
    bsl::string s = "lorem ipsum dolor sit amet" + bsl::string(16 * 1024, 'x');
    bsl::shared_ptr<bsl::vector<uint8_t> > data =
        bsl::make_shared<bsl::vector<uint8_t> >(s.cbegin(), s.cend());
    rmqt::Properties props = rmqt::Message(data).properties();
    props.headers          = bsl::make_shared<rmqt::FieldTable>();

    // Build message
    BasicMessageTransform t1;
    rmqt::Result<rmqp::MessageTransformer> t2 =
        rmqa::CompressionTransformer::create();
    ASSERT_TRUE(t2);

    t1.transform(data, props);
    EXPECT_THAT(data->size(), Eq(s.size()));
    EXPECT_THAT((*data)[0], Eq('x'));
    t2.value()->transform(data, props);
    EXPECT_THAT(data->size(), Lt(s.size()));

    // Unbuild message
    t2.value()->inverseTransform(data, props);
    EXPECT_THAT(data->size(), Eq(s.size()));
    EXPECT_THAT((*data)[0], Eq('x'));
    t1.inverseTransform(data, props);
    EXPECT_THAT(data->size(), Eq(s.size()));
    for (uint64_t i = 0; i < data->size(); ++i) {
        EXPECT_THAT((*data)[i], Eq(s[i]));
    }
}
#endif // RMQCPP_ENABLE_COMPRESSION

TEST(MessageBuilderTests, ChainedTransformIsValid)
{
    // Setup
    bsl::string s = "lorem ipsum dolor sit amet" + bsl::string(16 * 1024, 'x');
    bsl::shared_ptr<bsl::vector<uint8_t> > data =
        bsl::make_shared<bsl::vector<uint8_t> >(s.cbegin(), s.cend());
    rmqt::Properties props = rmqt::Message(data).properties();
    props.headers          = bsl::make_shared<rmqt::FieldTable>();

    // Build message
    BasicMessageTransform t1;

    t1.transform(data, props);
    t1.transform(data, props);
    EXPECT_THAT(data->size(), Eq(s.size()));
    for (uint64_t i = 0; i < data->size(); ++i) {
        EXPECT_THAT((*data)[i], Eq(s[i]));
    }

    // Unbuild message
    t1.inverseTransform(data, props);
    t1.inverseTransform(data, props);
    EXPECT_THAT(data->size(), Eq(s.size()));
    for (uint64_t i = 0; i < data->size(); ++i) {
        EXPECT_THAT((*data)[i], Eq(s[i]));
    }
}
