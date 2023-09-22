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

#include <rmqamqpt_types.h>
#include <rmqamqpt_writer.h>

#include <rmqamqpt_buffer.h>
#include <rmqt_fieldvalue.h>

#include <bdlb_bigendian.h>
#include <bdlt_datetime.h>

#include <bsl_cstring.h>
#include <bsl_iostream.h>
#include <bsl_limits.h>
#include <bsl_vector.h>

#include <bsl_iosfwd.h>
#include <bsl_map.h>
#include <bsl_memory.h>
#include <bsl_string.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqpt;

using namespace ::testing;

namespace {

// this is a real response from a RabbitMQ server
/*
==> Fieldtable
capabilities: [publisher_confirms: 1, exchange_exchange_bindings: 1,
               basic.nack: 1, consumer_cancel_notify: 1, connection.blocked: 1,
               consumer_priorities: 1, authentication_failure_close: 1,
               per_consumer_qos: 1, direct_reply_to: 1],
cluster_name: "rabbit@rabbit1", copyright: "Copyright (C) 2007-2016 Pivotal
 Software, Inc.",
information: "Licensed under the MPL.  See http://www.rabbitmq.com/",
platform: "Erlang/OTP",
product: "RabbitMQ", version: "3.6.2"
 */
const uint8_t serverProps[] = {
    0x00, 0x00, 0x01, 0xBB, 12,  99,  97,  112,  97,  98,  105, 108, 105, 116,
    105,  101,  115,  70,   0,   0,   0,   0xC7, 18,  112, 117, 98,  108, 105,
    115,  104,  101,  114,  95,  99,  111, 110,  102, 105, 114, 109, 115, 116,
    1,    26,   101,  120,  99,  104, 97,  110,  103, 101, 95,  101, 120, 99,
    104,  97,   110,  103,  101, 95,  98,  105,  110, 100, 105, 110, 103, 115,
    116,  1,    10,   98,   97,  115, 105, 99,   46,  110, 97,  99,  107, 116,
    1,    22,   99,   111,  110, 115, 117, 109,  101, 114, 95,  99,  97,  110,
    99,   101,  108,  95,   110, 111, 116, 105,  102, 121, 116, 1,   18,  99,
    111,  110,  110,  101,  99,  116, 105, 111,  110, 46,  98,  108, 111, 99,
    107,  101,  100,  116,  1,   19,  99,  111,  110, 115, 117, 109, 101, 114,
    95,   112,  114,  105,  111, 114, 105, 116,  105, 101, 115, 116, 1,   28,
    97,   117,  116,  104,  101, 110, 116, 105,  99,  97,  116, 105, 111, 110,
    95,   102,  97,   105,  108, 117, 114, 101,  95,  99,  108, 111, 115, 101,
    116,  1,    16,   112,  101, 114, 95,  99,   111, 110, 115, 117, 109, 101,
    114,  95,   113,  111,  115, 116, 1,   15,   100, 105, 114, 101, 99,  116,
    95,   114,  101,  112,  108, 121, 95,  116,  111, 116, 1,   12,  99,  108,
    117,  115,  116,  101,  114, 95,  110, 97,   109, 101, 83,  0,   0,   0,
    14,   114,  97,   98,   98,  105, 116, 64,   114, 97,  98,  98,  105, 116,
    49,   9,    99,   111,  112, 121, 114, 105,  103, 104, 116, 83,  0,   0,
    0,    46,   67,   111,  112, 121, 114, 105,  103, 104, 116, 32,  40,  67,
    41,   32,   50,   48,   48,  55,  45,  50,   48,  49,  54,  32,  80,  105,
    118,  111,  116,  97,   108, 32,  83,  111,  102, 116, 119, 97,  114, 101,
    44,   32,   73,   110,  99,  46,  11,  105,  110, 102, 111, 114, 109, 97,
    116,  105,  111,  110,  83,  0,   0,   0,    53,  76,  105, 99,  101, 110,
    115,  101,  100,  32,   117, 110, 100, 101,  114, 32,  116, 104, 101, 32,
    77,   80,   76,   46,   32,  32,  83,  101,  101, 32,  104, 116, 116, 112,
    58,   47,   47,   119,  119, 119, 46,  114,  97,  98,  98,  105, 116, 109,
    113,  46,   99,   111,  109, 47,  8,   112,  108, 97,  116, 102, 111, 114,
    109,  83,   0,    0,    0,   10,  69,  114,  108, 97,  110, 103, 47,  79,
    84,   80,   7,    112,  114, 111, 100, 117,  99,  116, 83,  0,   0,   0,
    8,    82,   97,   98,   98,  105, 116, 77,   81,  7,   118, 101, 114, 115,
    105,  111,  110,  83,   0,   0,   0,   5,    51,  46,  54,  46,  50};

// this is a real value sent from the pika library
/*
==> Fieldtable
platform: "Python 2.7.11", product: "Pika Python Client Library",
version: "0.10.0",
capabilities: [connection.blocked: 1, authentication_failure_close: 1,
               consumer_cancel_notify: 1, publisher_confirms: 1,
               basic.nack: 1],
information: "See http://pika.rtfd.org"
*/
const uint8_t clientProps[] = {
    8,   112, 108, 97,  116, 102, 111, 114, 109, 83,  0,   0,   0,   13,  80,
    121, 116, 104, 111, 110, 32,  50,  46,  55,  46,  49,  49,  7,   112, 114,
    111, 100, 117, 99,  116, 83,  0,   0,   0,   26,  80,  105, 107, 97,  32,
    80,  121, 116, 104, 111, 110, 32,  67,  108, 105, 101, 110, 116, 32,  76,
    105, 98,  114, 97,  114, 121, 7,   118, 101, 114, 115, 105, 111, 110, 83,
    0,   0,   0,   6,   48,  46,  49,  48,  46,  48,  12,  99,  97,  112, 97,
    98,  105, 108, 105, 116, 105, 101, 115, 70,  0,   0,   0,   111, 18,  99,
    111, 110, 110, 101, 99,  116, 105, 111, 110, 46,  98,  108, 111, 99,  107,
    101, 100, 116, 1,   28,  97,  117, 116, 104, 101, 110, 116, 105, 99,  97,
    116, 105, 111, 110, 95,  102, 97,  105, 108, 117, 114, 101, 95,  99,  108,
    111, 115, 101, 116, 1,   22,  99,  111, 110, 115, 117, 109, 101, 114, 95,
    99,  97,  110, 99,  101, 108, 95,  110, 111, 116, 105, 102, 121, 116, 1,
    18,  112, 117, 98,  108, 105, 115, 104, 101, 114, 95,  99,  111, 110, 102,
    105, 114, 109, 115, 116, 1,   10,  98,  97,  115, 105, 99,  46,  110, 97,
    99,  107, 116, 1,   11,  105, 110, 102, 111, 114, 109, 97,  116, 105, 111,
    110, 83,  0,   0,   0,   24,  83,  101, 101, 32,  104, 116, 116, 112, 58,
    47,  47,  112, 105, 107, 97,  46,  114, 116, 102, 100, 46,  111, 114, 103};

/*
Taken from packet capture from RabbitMQ 3.8.16

==> FieldTable
    tags (array)
    description (string):
    user_who_performed_action (string): rmq-lib
    cluster_state (field table) <...>
    tracing (boolean): false
    metadata (string): #{description => <<>>,tags => []}
    tags (array)
    description (string):
    name (string): bhjkdsabhjkdsa
    timestamp_in_ms (long int): 1625857949209
*/
const uint8_t duplicateKeyFieldTable[] = {
    0x00, 0x00, 0x00, 0xff, 0x04, 0x74, 0x61, 0x67, 0x73, 0x41, 0x00, 0x00,
    0x00, 0x09, 0x53, 0x00, 0x00, 0x00, 0x04, 0x62, 0x6c, 0x61, 0x68, 0x0b,
    0x64, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x53,
    0x00, 0x00, 0x00, 0x00, 0x19, 0x75, 0x73, 0x65, 0x72, 0x5f, 0x77, 0x68,
    0x6f, 0x5f, 0x70, 0x65, 0x72, 0x66, 0x6f, 0x72, 0x6d, 0x65, 0x64, 0x5f,
    0x61, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x53, 0x00, 0x00, 0x00, 0x07, 0x72,
    0x6d, 0x71, 0x2d, 0x6c, 0x69, 0x62, 0x0d, 0x63, 0x6c, 0x75, 0x73, 0x74,
    0x65, 0x72, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x65, 0x46, 0x00, 0x00, 0x00,
    0x18, 0x0b, 0x74, 0x65, 0x73, 0x74, 0x40, 0x72, 0x61, 0x62, 0x62, 0x69,
    0x74, 0x53, 0x00, 0x00, 0x00, 0x07, 0x72, 0x75, 0x6e, 0x6e, 0x69, 0x6e,
    0x67, 0x07, 0x74, 0x72, 0x61, 0x63, 0x69, 0x6e, 0x67, 0x74, 0x00, 0x08,
    0x6d, 0x65, 0x74, 0x61, 0x64, 0x61, 0x74, 0x61, 0x53, 0x00, 0x00, 0x00,
    0x25, 0x23, 0x7b, 0x64, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69,
    0x6f, 0x6e, 0x20, 0x3d, 0x3e, 0x20, 0x3c, 0x3c, 0x3e, 0x3e, 0x2c, 0x74,
    0x61, 0x67, 0x73, 0x20, 0x3d, 0x3e, 0x20, 0x5b, 0x62, 0x6c, 0x61, 0x68,
    0x5d, 0x7d, 0x04, 0x74, 0x61, 0x67, 0x73, 0x41, 0x00, 0x00, 0x00, 0x09,
    0x53, 0x00, 0x00, 0x00, 0x04, 0x62, 0x6c, 0x61, 0x68, 0x0b, 0x64, 0x65,
    0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x53, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x6e, 0x61, 0x6d, 0x65, 0x53, 0x00, 0x00, 0x00, 0x06,
    0x64, 0x73, 0x61, 0x64, 0x73, 0x61, 0x0f, 0x74, 0x69, 0x6d, 0x65, 0x73,
    0x74, 0x61, 0x6d, 0x70, 0x5f, 0x69, 0x6e, 0x5f, 0x6d, 0x73, 0x6c, 0x00,
    0x00, 0x01, 0x7a, 0x8c, 0x5f, 0x30, 0x6c};

typedef bsl::basic_stringstream<uint8_t> StringStream;
typedef bsl::basic_string<uint8_t> StringStreamBufferType;
typedef bsl::vector<uint8_t> BufferType;

} // namespace

TEST(TypesFieldTableDecode, ServerProps)
{
    BufferType buffer(sizeof(serverProps));
    bsl::memcpy(buffer.data(), serverProps, sizeof(serverProps));

    Buffer data(buffer.data(), sizeof(serverProps));
    rmqt::FieldTable ft;
    const bool decodable = rmqamqpt::Types::decodeFieldTable(&ft, &data);
    EXPECT_EQ(data.available(), 0);
    EXPECT_TRUE(decodable);

    bsl::map<bsl::string, rmqt::FieldValue> expect;
    expect["cluster_name"] = rmqt::FieldValue(bsl::string("rabbit@rabbit1"));
    expect["copyright"]    = rmqt::FieldValue(
        bsl::string("Copyright (C) 2007-2016 Pivotal Software, Inc."));
    expect["information"] = rmqt::FieldValue(
        bsl::string("Licensed under the MPL.  See http://www.rabbitmq.com/"));
    expect["platform"] = rmqt::FieldValue(bsl::string("Erlang/OTP"));
    expect["product"]  = rmqt::FieldValue(bsl::string("RabbitMQ"));
    expect["version"]  = rmqt::FieldValue(bsl::string("3.6.2"));

    bsl::shared_ptr<rmqt::FieldTable> capabilities =
        bsl::make_shared<rmqt::FieldTable>();
    (*capabilities)["publisher_confirms"]           = rmqt::FieldValue(true);
    (*capabilities)["exchange_exchange_bindings"]   = rmqt::FieldValue(true);
    (*capabilities)["basic.nack"]                   = rmqt::FieldValue(true);
    (*capabilities)["consumer_cancel_notify"]       = rmqt::FieldValue(true);
    (*capabilities)["connection.blocked"]           = rmqt::FieldValue(true);
    (*capabilities)["consumer_priorities"]          = rmqt::FieldValue(true);
    (*capabilities)["authentication_failure_close"] = rmqt::FieldValue(true);
    (*capabilities)["per_consumer_qos"]             = rmqt::FieldValue(true);
    (*capabilities)["direct_reply_to"]              = rmqt::FieldValue(true);
    expect["capabilities"]                          = capabilities;

    EXPECT_THAT(ft, Eq(expect));

    bsl::vector<uint8_t> storage;
    rmqamqpt::Writer writer(&storage);
    rmqamqpt::Types::encodeFieldTable(writer, ft);

    EXPECT_EQ(storage.size(), sizeof(serverProps));
}

TEST(TypesFieldTableDecode, DuplicateKeyFieldTableDecodesOK)
{
    // Ignore the second 'tags' key which exists in duplicateKeyFieldTable.
    // Just use one of them

    BufferType buffer(sizeof(duplicateKeyFieldTable));
    bsl::memcpy(
        buffer.data(), duplicateKeyFieldTable, sizeof(duplicateKeyFieldTable));

    Buffer data(buffer.data(), sizeof(duplicateKeyFieldTable));
    rmqt::FieldTable ft;
    const bool decodable = rmqamqpt::Types::decodeFieldTable(&ft, &data);

    EXPECT_TRUE(decodable);

    EXPECT_TRUE(ft["tags"].is<bsl::shared_ptr<rmqt::FieldArray> >());

    bsl::shared_ptr<rmqt::FieldArray> array =
        ft["tags"].the<bsl::shared_ptr<rmqt::FieldArray> >();

    EXPECT_THAT(array->size(), Eq(1));
    EXPECT_TRUE((*array)[0].is<bsl::string>());
    EXPECT_THAT((*array)[0].the<bsl::string>(), Eq("blah"));
}

TEST(TypesFieldTableDecode, ClientProps)
{
    BufferType buffer(sizeof(clientProps) + 4);
    const bdlb::BigEndianUint32 length =
        bdlb::BigEndianUint32::make(sizeof(clientProps));
    bsl::memcpy(buffer.data(), &length, sizeof(length));
    bsl::memcpy(buffer.data() + sizeof(length), clientProps, length);

    Buffer data(buffer.data(), length + sizeof(length));
    rmqt::FieldTable ft;
    bool decodable = rmqamqpt::Types::decodeFieldTable(&ft, &data);
    EXPECT_EQ(data.available(), 0);
    EXPECT_TRUE(decodable);

    bsl::map<bsl::string, rmqt::FieldValue> expect;
    expect["information"] =
        rmqt::FieldValue(bsl::string("See http://pika.rtfd.org"));
    expect["platform"] = rmqt::FieldValue(bsl::string("Python 2.7.11"));
    expect["product"] =
        rmqt::FieldValue(bsl::string("Pika Python Client Library"));
    expect["version"] = rmqt::FieldValue(bsl::string("0.10.0"));

    bsl::shared_ptr<rmqt::FieldTable> capabilities =
        bsl::make_shared<rmqt::FieldTable>();
    (*capabilities)["connection.blocked"]           = rmqt::FieldValue(true);
    (*capabilities)["authentication_failure_close"] = rmqt::FieldValue(true);
    (*capabilities)["consumer_cancel_notify"]       = rmqt::FieldValue(true);
    (*capabilities)["publisher_confirms"]           = rmqt::FieldValue(true);
    (*capabilities)["connection.blocked"]           = rmqt::FieldValue(true);
    (*capabilities)["basic.nack"]                   = rmqt::FieldValue(true);
    expect["capabilities"]                          = capabilities;

    EXPECT_THAT(ft, Eq(expect));

    BufferType storage;
    rmqamqpt::Writer writer(&storage);
    rmqamqpt::Types::encodeFieldTable(writer, ft);
}

TEST(TypesEncoding, ShouldRoundTripShortStringCorrectly)
{
    // GIVEN
    const bsl::string shortString("ThisIsAShortString");

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    // WHEN
    rmqamqpt::Types::encodeShortString(writer, shortString);

    StringStreamBufferType data(storage.data(), storage.size());

    rmqamqpt::Buffer buffer(data.begin(), data.size());
    bsl::string resultString;
    bool result = rmqamqpt::Types::decodeShortString(&resultString, &buffer);

    // THEN
    EXPECT_TRUE(result);
    EXPECT_EQ(shortString, resultString);
}

TEST(TypesEncoding, ShouldTruncateShortString)
{
    // GIVEN
    const bsl::string longString(
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!");

    // WHEN
    bsl::vector<uint8_t> output;
    rmqamqpt::Writer writer(&output);
    rmqamqpt::Types::encodeShortString(writer, longString);

    // THEN

    // 255 max string length + 1 byte for size
    const bsl::size_t EXPECTED_SIZE = 1 + 255;
    EXPECT_THAT(output.size(), Eq(EXPECTED_SIZE));
}

TEST(TypesEncoding, ShouldRoundTripLongStringCorrectly)
{
    // GIVEN
    const bsl::string longString(
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!"
        "ThisIsALongerString!ThisIsALongerString!ThisIsALongerString!");

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    // WHEN
    rmqamqpt::Types::encodeLongString(writer, longString);

    StringStreamBufferType data(storage.data(), storage.size());
    rmqamqpt::Buffer buffer(data.begin(), data.size());
    bsl::string resultString;
    bool result = rmqamqpt::Types::decodeLongString(&resultString, &buffer);

    // THEN
    EXPECT_TRUE(result);
    EXPECT_EQ(longString, resultString);
}

TEST(TypesEncoding, ShouldRoundTripByteVectorCorrectly)
{
    // GIVEN
    const bsl::uint8_t byteArray[] = {10, 20, 30, 40, 50, 60, 70, 80};
    const BufferType byteVector(bsl::begin(byteArray), bsl::end(byteArray));

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    // WHEN
    rmqamqpt::Types::encodeByteVector(writer, byteVector);

    StringStreamBufferType data(storage.data(), storage.size());
    rmqamqpt::Buffer buffer(data.begin(), data.size());
    BufferType resultVector;
    bool result = rmqamqpt::Types::decodeByteVector(
        &resultVector, &buffer, byteVector.size());

    // THEN
    EXPECT_TRUE(result);
    EXPECT_EQ(byteVector, resultVector);
}

TEST(TypesEncoding, ShouldRoundTripFieldValueBoolCorrectly)
{
    // GIVEN
    const rmqt::FieldValue fieldValue(true);

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    // WHEN
    rmqamqpt::Types::encodeFieldValue(writer, fieldValue);

    StringStreamBufferType data(storage.data(), storage.size());
    rmqamqpt::Buffer buffer(data.begin(), data.size());
    rmqt::FieldValue resultFieldValue(false);
    const bool result =
        rmqamqpt::Types::decodeFieldValue(&resultFieldValue, &buffer);

    // THEN
    EXPECT_TRUE(result);
    EXPECT_EQ(fieldValue, resultFieldValue);
}

TEST(TypesEncoding, ShouldRoundTripFieldValueByteArrayCorrectly)
{
    // GIVEN
    const bsl::uint8_t byteArrayData[] = {10, 20, 30, 40, 50, 60, 70, 80};
    const bsl::vector<bsl::uint8_t> byteArray(bsl::begin(byteArrayData),
                                              bsl::end(byteArrayData));
    const rmqt::FieldValue fieldValue(byteArray);

    const bsl::uint8_t expectedBytes[] = {
        'x', 0, 0, 0, 8, 10, 20, 30, 40, 50, 60, 70, 80};

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    // WHEN
    rmqamqpt::Types::encodeFieldValue(writer, fieldValue);

    StringStreamBufferType data(storage.data(), storage.size());
    const bsl::vector<bsl::uint8_t> actualData(data.begin(), data.end());

    // THEN
    const bsl::vector<bsl::uint8_t> expectedData(bsl::begin(expectedBytes),
                                                 bsl::end(expectedBytes));
    ASSERT_EQ(actualData, expectedData);

    // WHEN
    rmqamqpt::Buffer buffer(data.begin(), data.size());
    rmqt::FieldValue resultFieldValue;
    const bool result =
        rmqamqpt::Types::decodeFieldValue(&resultFieldValue, &buffer);

    // THEN
    ASSERT_TRUE(result);
    ASSERT_TRUE(fieldValue.is<bsl::vector<bsl::uint8_t> >());
    ASSERT_EQ(fieldValue.the<bsl::vector<bsl::uint8_t> >().size(),
              byteArray.size());
    ASSERT_EQ(fieldValue.the<bsl::vector<bsl::uint8_t> >(), byteArray);
    ASSERT_EQ(fieldValue, resultFieldValue);
}

TEST(TypesEncoding, ShouldRoundTripFieldValueArrayCorrectly)
{
    // GIVEN
    const rmqt::FieldValue fieldValue1(true);
    const rmqt::FieldValue fieldValue2(false);
    const rmqt::FieldValue fieldValue3(true);

    const uint8_t expectedBytes[] = {
        'A', 0x00, 0x00, 0x00, 0x06, 't', 0x01, 't', 0x00, 't', 0x01};

    bsl::shared_ptr<rmqt::FieldArray> fieldArray =
        bsl::make_shared<rmqt::FieldArray>();
    fieldArray->push_back(fieldValue1);
    fieldArray->push_back(fieldValue2);
    fieldArray->push_back(fieldValue3);
    const rmqt::FieldValue fieldValue(fieldArray);

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    // WHEN
    rmqamqpt::Types::encodeFieldValue(writer, fieldValue);

    StringStreamBufferType data(storage.data(), storage.size());

    EXPECT_EQ(sizeof(expectedBytes), data.size());

    for (bsl::size_t i = 0; i < data.size(); i++) {
        EXPECT_EQ(expectedBytes[i], data[i]);
    }

    rmqamqpt::Buffer buffer(data.begin(), data.size());
    rmqt::FieldValue resultFieldValue;
    const bool result =
        rmqamqpt::Types::decodeFieldValue(&resultFieldValue, &buffer);

    // THEN
    EXPECT_TRUE(result);
    EXPECT_TRUE(resultFieldValue.is<bsl::shared_ptr<rmqt::FieldArray> >());

    EXPECT_EQ(*fieldValue.the<bsl::shared_ptr<rmqt::FieldArray> >(),
              *resultFieldValue.the<bsl::shared_ptr<rmqt::FieldArray> >());
}

TEST(TypesEncoding, ShouldRoundTripFieldValueArrayWithFloatCorrectly)
{
    // GIVEN
    const rmqt::FieldValue fieldValueFloat32_0(float(0.0));
    const rmqt::FieldValue fieldValueFloat32_1(float(42.0));
    const rmqt::FieldValue fieldValueFloat32_2(float(-1.0));
    const rmqt::FieldValue fieldValueFloat32_3(float(1.0));
    const rmqt::FieldValue fieldValueFloat32_4(float(2.125));

    const rmqt::FieldValue fieldValueFloat64_0(double(0.0));
    const rmqt::FieldValue fieldValueFloat64_1(double(42.0));
    const rmqt::FieldValue fieldValueFloat64_2(double(-1.0));
    const rmqt::FieldValue fieldValueFloat64_3(double(1.0));
    const rmqt::FieldValue fieldValueFloat64_4(double(2.125));

    // See https://gregstoll.com/~gregstoll/floattohex/
    const uint8_t expectedBytes[] = {
        // Array with 10 items, total 70 bytes (hex=0x46):
        // (5 * (1 + sizeof(float)) +
        // (5 * (1 + sizeof(double))
        'A', 0x00, 0x00, 0x00, 0x46, 'f',  0x00, 0x00, 0x00, 0x00, // float: 0.0
        'd', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // double: 0.0
        'f', 0x42, 0x28, 0x00, 0x00,                         // float: 42.0
        'd', 0x40, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // double: 42.0
        'f', 0xbf, 0x80, 0x00, 0x00,                         // float: -1.0
        'd', 0xbf, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // double: -1.0
        'f', 0x3f, 0x80, 0x00, 0x00,                         // float: 1.0
        'd', 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // double: 1.0
        'f', 0x40, 0x08, 0x00, 0x00,                         // float: 2.125
        'd', 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // double: 2.125
    };

    bsl::shared_ptr<rmqt::FieldArray> fieldArray =
        bsl::make_shared<rmqt::FieldArray>();
    fieldArray->push_back(fieldValueFloat32_0);
    fieldArray->push_back(fieldValueFloat64_0);

    fieldArray->push_back(fieldValueFloat32_1);
    fieldArray->push_back(fieldValueFloat64_1);

    fieldArray->push_back(fieldValueFloat32_2);
    fieldArray->push_back(fieldValueFloat64_2);

    fieldArray->push_back(fieldValueFloat32_3);
    fieldArray->push_back(fieldValueFloat64_3);

    fieldArray->push_back(fieldValueFloat32_4);
    fieldArray->push_back(fieldValueFloat64_4);

    const rmqt::FieldValue fieldValue(fieldArray);

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    // WHEN
    rmqamqpt::Types::encodeFieldValue(writer, fieldValue);

    StringStreamBufferType data(storage.data(), storage.size());

    const BufferType expectedBytesArray(bsl::begin(expectedBytes),
                                        bsl::end(expectedBytes));
    const BufferType actualBytesArray(bsl::begin(data), bsl::end(data));
    EXPECT_EQ(expectedBytesArray, actualBytesArray);

    rmqamqpt::Buffer buffer(data.begin(), data.size());
    rmqt::FieldValue resultFieldValue;
    const bool result =
        rmqamqpt::Types::decodeFieldValue(&resultFieldValue, &buffer);

    // THEN
    EXPECT_TRUE(result);
    EXPECT_TRUE(resultFieldValue.is<bsl::shared_ptr<rmqt::FieldArray> >());

    EXPECT_EQ(*fieldValue.the<bsl::shared_ptr<rmqt::FieldArray> >(),
              *resultFieldValue.the<bsl::shared_ptr<rmqt::FieldArray> >());
}

TEST(TypesEncoding, ShouldRoundTripFieldArrayStringsAndBytesCorrectly)
{
    // GIVEN
    const bsl::uint8_t byteArrayData[] = {42, 100, 255, 0, 128};
    const bsl::vector<bsl::uint8_t> byteArray(bsl::begin(byteArrayData),
                                              bsl::end(byteArrayData));

    bsl::string strval = "ThisIsAString!";
    const rmqt::FieldValue fieldValue1(strval);
    const rmqt::FieldValue fieldValue2(true);
    const rmqt::FieldValue fieldValue3(byteArray);

    rmqt::FieldArray fieldArray;
    fieldArray.push_back(fieldValue1);
    fieldArray.push_back(fieldValue2);
    fieldArray.push_back(fieldValue3);

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    // WHEN
    rmqamqpt::Types::encodeFieldArray(writer, fieldArray);

    StringStreamBufferType data(storage.data(), storage.size());
    rmqamqpt::Buffer buffer(data.begin(), data.size());
    rmqt::FieldArray resultFieldArray;
    bool result = rmqamqpt::Types::decodeFieldArray(&resultFieldArray, &buffer);

    // THEN
    EXPECT_TRUE(result);
    EXPECT_EQ(byteArray.size(), 5);
    EXPECT_EQ(fieldValue3.the<bsl::vector<bsl::uint8_t> >().size(), 5);
    EXPECT_EQ(fieldArray, resultFieldArray);
}

TEST(TypesEncoding, EmptyFieldTable)
{
    const rmqt::FieldTable empty;

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    rmqamqpt::Types::encodeFieldTable(writer, empty);

    StringStreamBufferType str(storage.data(), storage.size());

    EXPECT_EQ(str[0], 0);
    EXPECT_EQ(str[1], 0);
    EXPECT_EQ(str[2], 0);
    EXPECT_EQ(str[3], 0);
}

TEST(TypesEncoding, Timestamp)
{
    const bdlt::Datetime millennium(2000, 1, 1);

    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    rmqamqpt::Types::encodeTimestamp(writer, millennium);

    StringStreamBufferType str(storage.data(), storage.size());

    bdlb::BigEndianInt64 expected = bdlb::BigEndianInt64::make(946684800);
    bsl::uint8_t* inspect         = reinterpret_cast<bsl::uint8_t*>(&expected);

    EXPECT_EQ(str.size(), 8);
    for (bsl::size_t i = 0; i < str.size(); ++i) {
        EXPECT_THAT(str[i], Eq(inspect[i]));
    }

    bdlt::Datetime decoded;
    rmqamqpt::Buffer buf(str.begin(), str.size());
    rmqamqpt::Types::decodeTimestamp(&decoded, &buf), Eq(true);

    EXPECT_THAT(decoded, Eq(millennium));
}

rmqt::FieldValue roundTripFieldValue(bool* decodeResult,
                                     const rmqt::FieldValue& fv)
{
    BufferType storage;
    rmqamqpt::Writer writer(&storage);

    rmqamqpt::Types::encodeFieldValue(writer, fv);

    StringStreamBufferType data(storage.data(), storage.size());
    rmqamqpt::Buffer buffer(data.begin(), data.size());
    rmqt::FieldValue resultFieldValue(false);
    *decodeResult =
        rmqamqpt::Types::decodeFieldValue(&resultFieldValue, &buffer);

    return resultFieldValue;
}

TEST(TypesEncodingMigration, EncodeNewDecodesOk)
{
    // New encoding, new decoding treats uint64_t as int64_t
    const rmqt::FieldValue minusOne(bsl::int64_t(-1));
    const rmqt::FieldValue uint64max =
        rmqt::FieldValue(bsl::numeric_limits<bsl::uint64_t>::max());

    bool decodeResult = false;
    const rmqt::FieldValue minusOneDecoded =
        roundTripFieldValue(&decodeResult, minusOne);
    EXPECT_TRUE(decodeResult);
    const rmqt::FieldValue uint64maxDecoded =
        roundTripFieldValue(&decodeResult, uint64max);
    EXPECT_TRUE(decodeResult);

    EXPECT_THAT(minusOneDecoded, Eq(minusOne));
    EXPECT_THAT(uint64maxDecoded, Eq(minusOne));
}
