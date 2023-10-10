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

#include <rmqt_message.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_memory.h>
#include <bsl_sstream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(MessageTests, Breathing)
{
    rmqt::Message msg;
    msg.guid();
    EXPECT_FALSE(msg.headers());
    EXPECT_TRUE(msg.messageId().empty());
    EXPECT_FALSE(msg.payload());
    EXPECT_EQ(msg.payloadSize(), 0);
    msg.properties();
    EXPECT_TRUE(msg.isPersistent());
}

TEST(MessageTests, BasicMessage)
{
    bsl::string foo = "foo";
    rmqt::Message msg(
        bsl::make_shared<bsl::vector<uint8_t> >(foo.begin(), foo.end()));
    EXPECT_FALSE(msg.messageId().empty());
    EXPECT_THAT(msg.payloadSize(), Eq(foo.size()));
}

TEST(MessageTests, FromProperties)
{
    bsl::string foo = "foo";
    rmqt::Properties props;
    props.contentType     = "text/plain; charset=us-ascii";
    props.contentEncoding = "7bit";
    rmqt::Message msg(
        bsl::make_shared<bsl::vector<uint8_t> >(foo.begin(), foo.end()), props);
    EXPECT_FALSE(msg.messageId().empty());
    EXPECT_THAT(msg.payloadSize(), Eq(foo.size()));
    EXPECT_THAT(msg.isPersistent(), Eq(false));
    EXPECT_THAT(msg.properties().contentEncoding.value(), Eq("7bit"));
    EXPECT_THAT(msg.properties().contentType.value(),
                Eq("text/plain; charset=us-ascii"));
}

TEST(MessageTests, UpdateProperties)
{
    bsl::string foo = "foo";
    rmqt::Message msg(
        bsl::make_shared<bsl::vector<uint8_t> >(foo.begin(), foo.end()));
    EXPECT_FALSE(msg.messageId().empty());
    EXPECT_THAT(msg.payloadSize(), Eq(foo.size()));
    EXPECT_THAT(msg.isPersistent(), Eq(true));
    msg.updateDeliveryMode(rmqt::DeliveryMode::NON_PERSISTENT);
    EXPECT_THAT(msg.isPersistent(), Eq(false));
    EXPECT_FALSE(msg.properties().priority);
    bsl::uint8_t priority_255(255), priority_1(1);
    msg.updateMessagePriority(priority_255);
    EXPECT_TRUE(msg.properties().priority);
    EXPECT_THAT(msg.properties().priority.value(), Eq(priority_255));
    msg.updateMessagePriority(priority_1);
    EXPECT_THAT(msg.properties().priority.value(), Eq(priority_1));
}
