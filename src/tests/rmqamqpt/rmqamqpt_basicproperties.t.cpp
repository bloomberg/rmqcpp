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

#include <rmqamqp_framer.h>
#include <rmqamqpt_basicproperties.h>
#include <rmqamqpt_writer.h>
#include <rmqt_properties.h>

#include <ball_log.h>

#include <bsl_cstdint.h>
#include <bsl_sstream.h>
#include <bsl_vector.h>

#include <bsl_memory.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.BASICPROPERTIES.TESTS")

void encodeDecode(rmqamqpt::BasicProperties& basicProps)
{
    bsl::vector<uint8_t> data;
    rmqamqpt::Writer writer(&data);
    basicProps.encode(writer, basicProps);

    BALL_LOG_INFO << "datasize: " << data.size();

    EXPECT_THAT(basicProps.decode(&basicProps, data.begin(), data.size()),
                Eq(true));
}

} // namespace

TEST(Methods_BasicProperties, Breathing)
{
    rmqamqpt::BasicProperties basicProps;
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0));
    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, ContentTypeSet)
{
    rmqt::Properties properties;
    properties.contentType = "Content";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x8000));
    EXPECT_THAT(basicProps.contentType(), Eq("Content"));
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, ContentTypeSetEncodeDecode)
{
    rmqt::Properties properties;
    properties.contentType = "Content";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);

    encodeDecode(basicProps);

    EXPECT_THAT(basicProps.contentType(), Eq("Content"));
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, ContentTypeAndTypeSetEncodeDecode)
{
    rmqt::Properties properties;
    properties.type        = "Type";
    properties.contentType = "Content";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);

    encodeDecode(basicProps);

    EXPECT_THAT(basicProps.contentType(), Eq("Content"));
    EXPECT_THAT(basicProps.type(), Eq("Type"));
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, ContentTypeAndTypeAndPrioritySetEncodeDecode)
{
    rmqt::Properties properties;
    properties.type        = "Type";
    properties.priority    = static_cast<uint8_t>(8);
    properties.contentType = "Content";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);

    encodeDecode(basicProps);

    EXPECT_THAT(basicProps.contentType(), Eq("Content"));
    EXPECT_THAT(basicProps.priority(), Eq(8));
    EXPECT_THAT(basicProps.type(), Eq("Type"));

    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, ContentEncoding)
{
    rmqt::Properties properties;
    properties.contentEncoding = "ContentEncoding";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x4000));
    EXPECT_THAT(basicProps.contentEncoding(), Eq("ContentEncoding"));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, Headers)
{
    rmqt::Properties properties;
    bsl::shared_ptr<rmqt::FieldTable> table(
        bsl::make_shared<rmqt::FieldTable>());
    properties.headers = table;
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);

    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x2000));
    EXPECT_THAT(basicProps.headers(), Eq(table));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, DeliveryMode)
{
    rmqt::Properties properties;
    properties.deliveryMode = static_cast<bsl::uint8_t>(2);
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x1000));
    EXPECT_THAT(basicProps.deliveryMode(), Eq(2));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, Priority)
{
    rmqt::Properties properties;
    properties.priority = static_cast<bsl::uint8_t>(5);
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0800));
    EXPECT_THAT(basicProps.priority(), Eq(5));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, CorrelationId)
{
    rmqt::Properties properties;
    properties.correlationId = "CorrelationId";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0400));
    EXPECT_THAT(basicProps.correlationId(), Eq("CorrelationId"));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, ReplyTo)
{
    rmqt::Properties properties;
    properties.replyTo = "ReplyTo";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0200));
    EXPECT_THAT(basicProps.replyTo(), Eq("ReplyTo"));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, Expiration)
{
    rmqt::Properties properties;
    properties.expiration = "Expiration";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0100));
    EXPECT_THAT(basicProps.expiration(), Eq("Expiration"));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, MessageId)
{
    rmqt::Properties properties;
    properties.messageId = "MsgId";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0080));
    EXPECT_THAT(basicProps.messageId(), Eq("MsgId"));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, Timestamp)
{
    bdlt::Datetime now(2020, 01, 31, 10, 50);
    rmqt::Properties properties;
    properties.timestamp = now;
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0040));
    EXPECT_THAT(basicProps.timestamp(), Eq(now));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, Type)
{
    rmqt::Properties properties;
    properties.type = "Type";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0020));
    EXPECT_THAT(basicProps.type(), Eq("Type"));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, UserId)
{
    rmqt::Properties properties;
    properties.userId = "UserId";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0010));
    EXPECT_THAT(basicProps.userId(), Eq("UserId"));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, AppId)
{
    rmqt::Properties properties;
    properties.appId = "AppId";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0008));
    EXPECT_THAT(basicProps.appId(), Eq("AppId"));

    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.messageId());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
}

TEST(Methods_BasicProperties, StreamOut)
{
    bdlt::Datetime now(2020, 01, 31, 10, 50);
    rmqt::Properties properties;
    properties.appId       = "AppId";
    properties.priority    = bsl::uint8_t(5);
    properties.timestamp   = now;
    properties.contentType = "application/xml";
    rmqamqpt::BasicProperties basicProps;
    basicProps.setProperties(properties);

    bsl::ostringstream oss;
    oss << basicProps;
    EXPECT_THAT(oss.str(),
                Eq("["
                   " flags = \"1000100001001000\""
                   " properties = ["
                   " contentType = application/xml"
                   " contentEncoding = NULL"
                   " headers = NULL"
                   " deliveryMode = \"non persistent\""
                   " priority = 0x05"
                   " correlationId = NULL"
                   " replyTo = NULL"
                   " expiration = NULL"
                   " messageId = NULL"
                   " timestamp = 31JAN2020_10:50:00.000000"
                   " type = NULL"
                   " userId = NULL"
                   " appId = AppId ] ]"));
}

TEST(Methods_BasicProperties, FromRMQT)
{
    rmqt::Properties rmqtProps;
    rmqamqpt::BasicProperties basicProps(rmqtProps);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0));
    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, FromRMQTMessageId)
{
    rmqt::Properties rmqtProps;
    rmqtProps.messageId = "foo";
    rmqamqpt::BasicProperties basicProps(rmqtProps);
    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x0080));
    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.deliveryMode());
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_THAT(basicProps.messageId().value(), Eq("foo"));
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}

TEST(Methods_BasicProperties, FromMessage)
{
    bsl::string rawData = "hello";
    rmqt::Message msg(
        bsl::make_shared<bsl::vector<uint8_t> >(rawData.begin(), rawData.end()),
        "myMessageId");
    rmqamqpt::BasicProperties basicProps(msg.properties());

    EXPECT_THAT(basicProps.propertyFlags(), Eq(0x1000 | 0x0080));
    EXPECT_FALSE(basicProps.headers());
    EXPECT_FALSE(basicProps.contentType());
    EXPECT_FALSE(basicProps.contentEncoding());
    EXPECT_THAT(basicProps.deliveryMode().value(),
                Eq(rmqt::DeliveryMode::PERSISTENT));
    EXPECT_FALSE(basicProps.priority());
    EXPECT_FALSE(basicProps.correlationId());
    EXPECT_FALSE(basicProps.replyTo());
    EXPECT_FALSE(basicProps.expiration());
    EXPECT_THAT(basicProps.messageId().value(), Eq("myMessageId"));
    EXPECT_FALSE(basicProps.timestamp());
    EXPECT_FALSE(basicProps.type());
    EXPECT_FALSE(basicProps.userId());
    EXPECT_FALSE(basicProps.appId());
}
