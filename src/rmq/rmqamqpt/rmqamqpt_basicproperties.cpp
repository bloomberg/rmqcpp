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

#include <rmqamqpt_basicproperties.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_types.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>
#include <rmqt_shortstring.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>
#include <bdlt_epochutil.h>
#include <bslim_printer.h>
#include <bsls_assert.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_functional.h>
#include <bsl_optional.h>

namespace BloombergLP {
namespace rmqamqpt {

namespace {

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.BASICPROPERTIES")

BSLMF_ASSERT(sizeof(uint8_t) == 1);
BSLMF_ASSERT(sizeof(int16_t) == 2);
BSLMF_ASSERT(sizeof(bdlt::EpochUtil::TimeT64) == 8);

enum PropertyType { SHORT_STRING, FIELD_TABLE, SHORT_SHORT_UINT, TIMESTAMP };

const PropertyType EXPECTED_TYPES[BasicProperties::NUM_BASIC_PROPERTIES] = {
    SHORT_STRING,
    SHORT_STRING,
    FIELD_TABLE,
    SHORT_SHORT_UINT,
    SHORT_SHORT_UINT,
    SHORT_STRING,
    SHORT_STRING,
    SHORT_STRING,
    SHORT_STRING,
    TIMESTAMP,
    SHORT_STRING,
    SHORT_STRING,
    SHORT_STRING};

const char* PROPERTY_NAMES[] = {"content-type",
                                "content-encoding",
                                "headers",
                                "delivery-mode",
                                "priority",
                                "correlation-id",
                                "reply-to",
                                "expiration",
                                "message-id",
                                "timestamp",
                                "type",
                                "user-id",
                                "app-id",
                                "reserved"};

uint16_t maskForProperty(uint8_t offset)
{
    const uint8_t shift   = 15 - offset;
    const uint16_t result = 1 << shift;
    return result;
}

template <typename T>
void maybeEncodeNullable(
    Writer& output,
    const bsl::optional<T>& nullableValue,
    const bsl::function<void(Writer&, const T&)>& encodeFunc)
{
    if (nullableValue) {
        encodeFunc(output, nullableValue.value());
    }
}

bsl::string getBitString(uint16_t value)
{
    const bsl::size_t kNumBits = 16;
    bsl::string result(kNumBits, '0');
    for (bsl::size_t i = 0; i < kNumBits; ++i) {
        result[i] = ((value & (0x8000 >> i)) == 0 ? '0' : '1');
    }
    return result;
}

} // namespace

uint16_t BasicProperties::propertyFlags() const
{
    uint16_t result = 0;
#define RMQAMQPT_PROPERTY_FLAGS(id, prop)                                      \
    if (d_properties.prop) {                                                   \
        const uint16_t mask = maskForProperty(id);                             \
        result |= mask;                                                        \
    }

    RMQAMQPT_PROPERTY_FLAGS(CONTENT_TYPE, contentType)
    RMQAMQPT_PROPERTY_FLAGS(CONTENT_ENCODING, contentEncoding)
    RMQAMQPT_PROPERTY_FLAGS(DELIVERY_MODE, deliveryMode)
    RMQAMQPT_PROPERTY_FLAGS(PRIORITY, priority)
    RMQAMQPT_PROPERTY_FLAGS(CORRELATION_ID, correlationId)
    RMQAMQPT_PROPERTY_FLAGS(REPLY_TO, replyTo)
    RMQAMQPT_PROPERTY_FLAGS(EXPIRATION, expiration)
    RMQAMQPT_PROPERTY_FLAGS(MESSAGE_ID, messageId)
    RMQAMQPT_PROPERTY_FLAGS(TIMESTAMP, timestamp)
    RMQAMQPT_PROPERTY_FLAGS(TYPE, type)
    RMQAMQPT_PROPERTY_FLAGS(USER_ID, userId)
    RMQAMQPT_PROPERTY_FLAGS(APP_ID, appId)

#undef RMQAMQPT_PROPERTY_FLAGS

    if (d_properties.headers) {
        const uint16_t mask = maskForProperty(HEADERS);
        result |= mask;
    }

    return result;
}

BasicProperties::BasicProperties()
: d_properties()
{
}

BasicProperties::BasicProperties(const rmqt::Properties& p)
: d_properties(p)
{
}

bsl::optional<bsl::string> BasicProperties::contentType() const
{
    return d_properties.contentType;
}
// MIME content type

bsl::optional<bsl::string> BasicProperties::contentEncoding() const
{
    return d_properties.contentEncoding;
}
// MIME content encoding

bsl::optional<bsl::shared_ptr<rmqt::FieldTable> >
BasicProperties::headers() const
{
    if (d_properties.headers)
        return d_properties.headers;
    return bsl::optional<bsl::shared_ptr<rmqt::FieldTable> >();
}
// message header field table

bsl::optional<bsl::uint8_t> BasicProperties::deliveryMode() const
{
    return d_properties.deliveryMode;
}
// non-persistent (1) or persistent (2)

bsl::optional<bsl::uint8_t> BasicProperties::priority() const
{
    return d_properties.priority;
}
// message priority, 0 to 9

bsl::optional<bsl::string> BasicProperties::correlationId() const
{
    return d_properties.correlationId;
}
// application correlation identifier

/**
 * FOR APP USE
 */

bsl::optional<bsl::string> BasicProperties::replyTo() const
{
    return d_properties.replyTo;
}
// address to reply to

bsl::optional<bsl::string> BasicProperties::expiration() const
{
    return d_properties.expiration;
}
// message expiration specification

bsl::optional<bsl::string> BasicProperties::messageId() const
{
    return d_properties.messageId;
}
// application message identifier

bsl::optional<bdlt::Datetime> BasicProperties::timestamp() const
{
    return d_properties.timestamp;
}
// message timestamp

bsl::optional<bsl::string> BasicProperties::type() const
{
    return d_properties.type;
}
// message type name

bsl::optional<bsl::string> BasicProperties::userId() const
{
    return d_properties.userId;
}
// creating user id

bsl::optional<bsl::string> BasicProperties::appId() const
{
    return d_properties.appId;
}
// creating application id

void BasicProperties::setProperties(const rmqt::Properties& properties)
{
    d_properties = properties;
}

template <typename T>
void writeErrorMsg(const bsl::string& propertyName, const T& value)
{
    BALL_LOG_ERROR << "Decoding fail for property: " << propertyName
                   << " with value: " << value;
}

bool BasicProperties::decode(BasicProperties* props,
                             const uint8_t* data,
                             bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    if (sizeof(uint16_t) > buffer.available()) {
        return false;
    }
    bool success         = true;
    const uint16_t flags = buffer.copy<bdlb::BigEndianUint16>();

    rmqt::Properties properties;

    if (flags & maskForProperty(CONTENT_TYPE)) {
        properties.contentType = "";
        if (!rmqamqpt::Types::decodeShortString(&*properties.contentType,
                                                &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[CONTENT_TYPE];
            success = false;
        }
    }
    if (flags & maskForProperty(CONTENT_ENCODING)) {
        properties.contentEncoding = "";
        if (!rmqamqpt::Types::decodeShortString(&*properties.contentEncoding,
                                                &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[CONTENT_ENCODING];
            success = false;
        }
    }
    if (flags & maskForProperty(HEADERS)) {
        properties.headers = bsl::make_shared<rmqt::FieldTable>();
        if (!rmqamqpt::Types::decodeFieldTable(properties.headers.ptr(),
                                               &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[HEADERS];
            success = false;
        }
    }
    if (flags & maskForProperty(DELIVERY_MODE)) {
        properties.deliveryMode = buffer.copy<uint8_t>();
    }
    if (flags & maskForProperty(PRIORITY)) {
        properties.priority = buffer.copy<uint8_t>();
    }
    if (flags & maskForProperty(CORRELATION_ID)) {
        properties.correlationId = "";
        if (!rmqamqpt::Types::decodeShortString(&*properties.correlationId,
                                                &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[CORRELATION_ID];
            success = false;
        }
    }
    if (flags & maskForProperty(REPLY_TO)) {
        properties.replyTo = "";
        if (!rmqamqpt::Types::decodeShortString(&*properties.replyTo,
                                                &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[REPLY_TO];
            success = false;
        }
    }
    if (flags & maskForProperty(EXPIRATION)) {
        properties.expiration = "";
        if (!rmqamqpt::Types::decodeShortString(&*properties.expiration,
                                                &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[EXPIRATION];
            success = false;
        }
    }
    if (flags & maskForProperty(MESSAGE_ID)) {
        properties.messageId = "";
        if (!rmqamqpt::Types::decodeShortString(&*properties.messageId,
                                                &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[MESSAGE_ID];
            success = false;
        }
    }
    if (flags & maskForProperty(TIMESTAMP)) {
        properties.timestamp = bdlt::Datetime();
        rmqamqpt::Types::decodeTimestamp(&*properties.timestamp, &buffer);
    }
    if (flags & maskForProperty(TYPE)) {
        properties.type = "";
        if (!rmqamqpt::Types::decodeShortString(&*properties.type, &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[TYPE];
            success = false;
        }
    }
    if (flags & maskForProperty(USER_ID)) {
        properties.userId = "";
        if (rmqamqpt::Types::decodeShortString(&*properties.userId, &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[USER_ID];
            success = false;
        }
    }
    if (flags & maskForProperty(APP_ID)) {
        properties.appId = "";
        if (!rmqamqpt::Types::decodeShortString(&*properties.appId, &buffer)) {
            BALL_LOG_ERROR << "Decoding fail for basic property: "
                           << PROPERTY_NAMES[APP_ID];
            success = false;
        }
    }

    if (success) {
        props->setProperties(properties);
    }

    const uint8_t MORE_PROPERTIES = 15;
    if (maskForProperty(MORE_PROPERTIES) & flags) {
        BALL_LOG_ERROR << "FAILED TO DECODE MORE PROPERTIES, SPEC EXTENDED?";
    }
    BALL_LOG_TRACE << "decoded props: " << *props;
    return success;
}

bsl::size_t BasicProperties::encodedSize() const
{
    bsl::size_t totalSize = sizeof(uint16_t);

    if (d_properties.contentType) {
        totalSize +=
            sizeof(uint8_t) + d_properties.contentType.value().length();
    }
    if (d_properties.contentEncoding) {
        totalSize +=
            sizeof(uint8_t) + d_properties.contentEncoding.value().length();
    }
    if (d_properties.headers) {
        const bsl::size_t tableLengthSize = sizeof(uint32_t);
        totalSize += tableLengthSize +
                     FieldValueUtil::encodedTableSize(*(d_properties.headers));
    }
    if (d_properties.deliveryMode) {
        totalSize += sizeof(d_properties.deliveryMode.value());
    }
    if (d_properties.priority) {
        totalSize += sizeof(d_properties.priority.value());
    }
    if (d_properties.correlationId) {
        totalSize +=
            sizeof(uint8_t) + d_properties.correlationId.value().length();
    }
    if (d_properties.replyTo) {
        totalSize += sizeof(uint8_t) + d_properties.replyTo.value().length();
    }
    if (d_properties.expiration) {
        totalSize += sizeof(uint8_t) + d_properties.expiration.value().length();
    }
    if (d_properties.messageId) {
        totalSize += sizeof(uint8_t) + d_properties.messageId.value().length();
    }
    if (d_properties.timestamp) {
        totalSize += sizeof(bdlt::EpochUtil::TimeT64);
    }
    if (d_properties.type) {
        totalSize += sizeof(uint8_t) + d_properties.type.value().length();
    }
    if (d_properties.userId) {
        totalSize += sizeof(uint8_t) + d_properties.userId.value().length();
    }
    if (d_properties.appId) {
        totalSize += sizeof(uint8_t) + d_properties.appId.value().length();
    }
    return totalSize;
}

void BasicProperties::encode(Writer& output, const BasicProperties& basicProps)
{
    const uint16_t flags = basicProps.propertyFlags();
    Types::write(output, bdlb::BigEndianUint16::make(flags));

    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.contentType,
                                     &rmqamqpt::Types::encodeShortString);
    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.contentEncoding,
                                     &rmqamqpt::Types::encodeShortString);

    if (basicProps.d_properties.headers) {
        rmqamqpt::Types::encodeFieldTable(output,
                                          *(basicProps.d_properties.headers));
    }
    maybeEncodeNullable<uint8_t>(output,
                                 basicProps.d_properties.deliveryMode,
                                 &rmqamqpt::Types::write<uint8_t>);
    maybeEncodeNullable<uint8_t>(output,
                                 basicProps.d_properties.priority,
                                 &rmqamqpt::Types::write<uint8_t>);
    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.correlationId,
                                     &rmqamqpt::Types::encodeShortString);
    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.replyTo,
                                     &rmqamqpt::Types::encodeShortString);
    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.expiration,
                                     &rmqamqpt::Types::encodeShortString);
    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.messageId,
                                     &rmqamqpt::Types::encodeShortString);
    maybeEncodeNullable<bdlt::Datetime>(output,
                                        basicProps.d_properties.timestamp,
                                        &rmqamqpt::Types::encodeTimestamp);
    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.type,
                                     &rmqamqpt::Types::encodeShortString);
    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.userId,
                                     &rmqamqpt::Types::encodeShortString);
    maybeEncodeNullable<bsl::string>(output,
                                     basicProps.d_properties.appId,
                                     &rmqamqpt::Types::encodeShortString);
}

rmqt::Properties BasicProperties::toProperties() const { return d_properties; }

bsl::ostream& operator<<(bsl::ostream& os, const BasicProperties::Id& id)
{
    BSLS_ASSERT(id < BasicProperties::NUM_BASIC_PROPERTIES);
    return os << PROPERTY_NAMES[id];
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicProperties& basicProps)
{
    const bsl::string flags(getBitString(basicProps.propertyFlags()));
    bslim::Printer printer(&os, 0, -1);
    printer.start();
    printer.printAttribute("flags", flags);
    printer.printAttribute("properties", basicProps.d_properties);
    printer.end();
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
