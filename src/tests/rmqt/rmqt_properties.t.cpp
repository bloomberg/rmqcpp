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

#include <rmqt_properties.h>

#include <rmqt_fieldvalue.h>

#include <bdlt_datetime.h>

#include <gtest/gtest.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_utility.h>

using namespace BloombergLP;
using namespace BloombergLP::rmqt;

namespace {

/// Every field set to a distinct non-default value, so that a field the copy
/// constructor or assignment operator forgets shows up as an inequality.
Properties fullyPopulated()
{
    Properties properties;
    properties.contentType     = bsl::string("application/json");
    properties.contentEncoding = bsl::string("gzip");
    properties.headers         = bsl::make_shared<FieldTable>();
    properties.headers->insert(
        bsl::make_pair(bsl::string("key"), bsl::string("original")));
    properties.deliveryMode  = DeliveryMode::PERSISTENT;
    properties.priority      = 7;
    properties.correlationId = bsl::string("correlation-id");
    properties.replyTo       = bsl::string("reply-to");
    properties.expiration    = bsl::string("60000");
    properties.messageId     = bsl::string("message-id");
    properties.timestamp     = bdlt::Datetime(2026, 9, 8, 12, 30, 15);
    properties.type          = bsl::string("type");
    properties.userId        = bsl::string("user-id");
    properties.appId         = bsl::string("app-id");
    return properties;
}

} // namespace

TEST(PropertiesTests, CopyConstructorCopiesEveryField)
{
    const Properties original = fullyPopulated();
    const Properties copy(original);

    EXPECT_EQ(copy, original);
}

TEST(PropertiesTests, CopyConstructorClonesHeaders)
{
    Properties original = fullyPopulated();
    Properties copy(original);

    EXPECT_NE(copy.headers.get(), original.headers.get());

    (*copy.headers)["key"] = bsl::string("changed");

    EXPECT_TRUE((*original.headers)["key"] ==
                FieldValue(bsl::string("original")));
}

TEST(PropertiesTests, CopyConstructorHandlesAbsentHeaders)
{
    Properties original = fullyPopulated();
    original.headers.reset();
    ASSERT_FALSE(original.headers);

    const Properties copy(original);

    EXPECT_FALSE(copy.headers);
    EXPECT_EQ(copy, original);
}

TEST(PropertiesTests, AssignmentCopiesEveryField)
{
    const Properties original = fullyPopulated();
    Properties assigned;
    assigned = original;

    EXPECT_EQ(assigned, original);
}

TEST(PropertiesTests, AssignmentClonesHeaders)
{
    Properties original = fullyPopulated();
    Properties assigned;
    assigned = original;

    EXPECT_NE(assigned.headers.get(), original.headers.get());

    (*assigned.headers)["key"] = bsl::string("changed");

    EXPECT_TRUE((*original.headers)["key"] ==
                FieldValue(bsl::string("original")));
}

TEST(PropertiesTests, SelfAssignmentKeepsHeaders)
{
    Properties properties = fullyPopulated();
    Properties& alias     = properties;

    properties = alias;

    ASSERT_TRUE(properties.headers);
    EXPECT_TRUE((*properties.headers)["key"] ==
                FieldValue(bsl::string("original")));
}
