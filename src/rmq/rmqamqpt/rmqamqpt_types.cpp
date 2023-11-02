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

#include <rmqamqpt_fieldvalue.h>

#include <rmqt_fieldvalue.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>
#include <bdlt_epochutil.h>
#include <bsls_assert.h>

#include <bsl_algorithm.h>
#include <bsl_cstddef.h>
#include <bsl_cstring.h>
#include <bsl_memory.h>
#include <bsl_utility.h>

namespace BloombergLP {
namespace rmqamqpt {

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.TYPES")

namespace {

template <typename T>
bool decodeString(bsl::string* string, rmqamqpt::Buffer* buffer)
{
    BSLS_ASSERT(string);
    BSLMF_ASSERT(sizeof(T) == sizeof(bsl::uint8_t) ||
                 sizeof(T) == sizeof(bsl::uint32_t));

    if (sizeof(T) > buffer->available()) {
        BALL_LOG_ERROR << "Not enough data to read string length";

        return false;
    }

    const T stringLength = buffer->copy<T>();

    if (stringLength > buffer->available()) {

        BALL_LOG_ERROR << "Not enough data to read string of length: "
                       << static_cast<unsigned int>(stringLength);
        return false;
    }

    rmqamqpt::Buffer stringBuffer = buffer->consume(stringLength);
    string->assign(reinterpret_cast<const char*>(stringBuffer.ptr()),
                   stringBuffer.available());
    return true;
}

} // namespace

bool Types::decodeLongString(bsl::string* string, rmqamqpt::Buffer* buffer)
{
    return decodeString<bdlb::BigEndianUint32>(string, buffer);
}

bool Types::decodeShortString(bsl::string* string, rmqamqpt::Buffer* buffer)
{
    return decodeString<bsl::uint8_t>(string, buffer);
}

void Types::encodeLongString(Writer& output, const bsl::string& s)
{
    Types::write(output, bdlb::BigEndianUint32::make(s.size()));
    if (!s.empty()) {
        output.write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }
}

void Types::encodeShortString(Writer& output, const bsl::string& s)
{
    // Truncate short strings longer than 255 characters
    const bsl::size_t sendSize = bsl::min(s.size(), bsl::size_t(255));

    Types::write(output, static_cast<bsl::uint8_t>(sendSize));
    if (!s.empty()) {
        output.write(reinterpret_cast<const uint8_t*>(s.data()), sendSize);
    }
}

bool Types::decodeByteVector(bsl::vector<bsl::uint8_t>* vector,
                             rmqamqpt::Buffer* buffer,
                             bsl::size_t bytes)
{
    BSLS_ASSERT(vector);

    if (bytes > buffer->available()) {
        return false;
    }

    if (bytes > 0) {
        rmqamqpt::Buffer::const_pointer ptr = buffer->ptr();
        vector->assign(ptr, ptr + bytes);
        buffer->skip(bytes);
    }
    else {
        vector->clear();
    }
    return true;
}

void Types::encodeByteVector(Writer& output, const bsl::vector<bsl::uint8_t>& v)
{
    if (!v.empty()) {
        output.write(v.data(), v.size());
    }
}

bool Types::decodeFieldValue(rmqt::FieldValue* outValue,
                             rmqamqpt::Buffer* buffer)
{
    BSLS_ASSERT(outValue);

    BSLMF_ASSERT(sizeof(bool) == 1);
    BSLMF_ASSERT(sizeof(bsl::int8_t) == 1);
    BSLMF_ASSERT(sizeof(bsl::uint8_t) == 1);

    BSLMF_ASSERT(sizeof(short) == 2);
    BSLMF_ASSERT(sizeof(bdlb::BigEndianInt16) == sizeof(short));

    BSLMF_ASSERT(sizeof(int) == 4);
    BSLMF_ASSERT(sizeof(bdlb::BigEndianInt32) == sizeof(int));

    BSLMF_ASSERT(sizeof(bsl::int64_t) == 8);
    BSLMF_ASSERT(sizeof(bdlb::BigEndianInt64) == sizeof(bsl::int64_t));

    BSLMF_ASSERT(sizeof(unsigned short) == 2);
    BSLMF_ASSERT(sizeof(bdlb::BigEndianUint16) == sizeof(unsigned short));

    BSLMF_ASSERT(sizeof(unsigned int) == 4);
    BSLMF_ASSERT(sizeof(bdlb::BigEndianUint32) == sizeof(unsigned int));

    BSLMF_ASSERT(sizeof(bsl::uint64_t) == 8);
    BSLMF_ASSERT(sizeof(bdlb::BigEndianUint64) == sizeof(bsl::uint64_t));

    BSLMF_ASSERT(sizeof(float) == sizeof(bdlb::BigEndianUint32));
    BSLMF_ASSERT(sizeof(double) == sizeof(bdlb::BigEndianUint64));

    if (buffer->available() < sizeof(bsl::uint8_t)) {
        return false;
    }

    const rmqamqpt::FieldValue::Type type =
        static_cast<rmqamqpt::FieldValue::Type>(buffer->copy<bsl::uint8_t>());

    switch (type) {
        case rmqamqpt::FieldValue::BOOLEAN: {
            if (buffer->available() < sizeof(bool)) {
                return false;
            }
            *outValue = rmqt::FieldValue(buffer->copy<bool>() != 0);
        } break;
        case rmqamqpt::FieldValue::SHORT_SHORT_INT: {
            if (buffer->available() < sizeof(bsl::int8_t)) {
                return false;
            }
            *outValue = static_cast<bsl::int8_t>(buffer->copy<bsl::int8_t>());
        } break;
        case rmqamqpt::FieldValue::SHORT_SHORT_UINT: {
            if (buffer->available() < sizeof(bsl::uint8_t)) {
                return false;
            }
            *outValue = static_cast<bsl::uint8_t>(buffer->copy<bsl::uint8_t>());
        } break;
        case rmqamqpt::FieldValue::SHORT_INT: {
            if (buffer->available() < sizeof(bdlb::BigEndianInt16)) {
                return false;
            }
            *outValue =
                static_cast<bsl::int16_t>(buffer->copy<bdlb::BigEndianInt16>());
        } break;
        case rmqamqpt::FieldValue::SHORT_UINT: {
            if (buffer->available() < sizeof(bdlb::BigEndianUint16)) {
                return false;
            }
            *outValue = static_cast<bsl::uint16_t>(
                buffer->copy<bdlb::BigEndianUint16>());
        } break;
        case rmqamqpt::FieldValue::LONG_INT: {
            if (buffer->available() < sizeof(bdlb::BigEndianInt32)) {
                return false;
            }
            *outValue =
                static_cast<bsl::int32_t>(buffer->copy<bdlb::BigEndianInt32>());
        } break;
        case rmqamqpt::FieldValue::LONG_UINT: {
            if (buffer->available() < sizeof(bdlb::BigEndianUint32)) {
                return false;
            }
            *outValue = static_cast<bsl::uint32_t>(
                buffer->copy<bdlb::BigEndianUint32>());
        } break;
        case rmqamqpt::FieldValue::AMQP_SPEC_LONG_LONG_INT: {
            if (buffer->available() < sizeof(bdlb::BigEndianInt64)) {
                return false;
            }
            *outValue =
                static_cast<bsl::int64_t>(buffer->copy<bdlb::BigEndianInt64>());
        } break;
        case rmqamqpt::FieldValue::RABBIT_SPEC_LONG_LONG_INT: {
            // https://www.rabbitmq.com/amqp-0-9-1-errata.html
            // RabbitMQ actually expects signed 64bit integer to be sent with
            // the 'l' flag, whereas the amqp 0.9.1 spec uses 'L'. The amqp
            // 0.9.1 spec uses 'l' for unsigned 64bit integers. rmqcpp does not
            // support unsigned 64bit integers, like RabbitMQ. But we decode
            // both 'L' and 'l' to signed 64bit integers for internal backwards
            // compatibility reasons.
            if (buffer->available() < sizeof(bdlb::BigEndianInt64)) {
                return false;
            }
            *outValue =
                static_cast<bsl::int64_t>(buffer->copy<bdlb::BigEndianInt64>());
        } break;
        case rmqamqpt::FieldValue::FLOAT: {
            if (buffer->available() < sizeof(bdlb::BigEndianUint32)) {
                return false;
            }
            const bsl::uint32_t temp = static_cast<bsl::uint32_t>(
                buffer->copy<bdlb::BigEndianUint32>());
            float floatValue;
            bsl::memcpy(&floatValue, &temp, sizeof(floatValue));
            *outValue = floatValue;
        } break;
        case rmqamqpt::FieldValue::DOUBLE: {
            if (buffer->available() < sizeof(bdlb::BigEndianUint64)) {
                return false;
            }
            const bsl::uint64_t temp = static_cast<bsl::uint64_t>(
                buffer->copy<bdlb::BigEndianUint64>());
            double doubleValue;
            bsl::memcpy(&doubleValue, &temp, sizeof(doubleValue));
            *outValue = doubleValue;
        } break;
        case rmqamqpt::FieldValue::DECIMAL: {
            BALL_LOG_ERROR << "Received unsupported decimal type";
            // TODO Handle Decimal
        } break;
        case rmqamqpt::FieldValue::LONG_STRING: {
            bsl::string val;
            if (!decodeLongString(&val, buffer)) {
                return false;
            }
            *outValue = val;
        } break;
        case rmqamqpt::FieldValue::BYTE_ARRAY: {
            if (buffer->available() < sizeof(bdlb::BigEndianUint32)) {
                return false;
            }
            const bsl::uint32_t arraySize = static_cast<bsl::uint32_t>(
                buffer->copy<bdlb::BigEndianUint32>());
            bsl::vector<bsl::uint8_t> val;
            if (!decodeByteVector(&val, buffer, arraySize)) {
                return false;
            }
            *outValue = val;
        } break;
        case rmqamqpt::FieldValue::FIELD_ARRAY: {
            bsl::shared_ptr<rmqt::FieldArray> val =
                bsl::make_shared<rmqt::FieldArray>();
            if (!decodeFieldArray(val.get(), buffer)) {
                return false;
            }
            *outValue = val;
        } break;
        case rmqamqpt::FieldValue::TIMESTAMP: {
            bdlt::Datetime timestamp;
            decodeTimestamp(&timestamp, buffer);
            *outValue = timestamp;
        } break;
        case rmqamqpt::FieldValue::FIELD_TABLE: {
            bsl::shared_ptr<rmqt::FieldTable> table =
                bsl::make_shared<rmqt::FieldTable>();
            if (!decodeFieldTable(table.get(), buffer)) {
                return false;
            }
            *outValue = table;
        } break;
        case rmqamqpt::FieldValue::NO_FIELD: {
            // There is no value to return.
            outValue->reset();
        } break;
        default: {
            return false;
        }
    }

    return true;
}

class FieldValueEncoder {
  public:
  private:
    Writer& output;

  public:
    explicit FieldValueEncoder(Writer& output)
    : output(output)
    {
    }

    void operator()(bool val) const
    {
        Types::write(output, FieldValue::BOOLEAN);
        Types::write(output, val);
    }
    void operator()(bsl::uint8_t val) const
    {
        Types::write(output, FieldValue::SHORT_SHORT_UINT);
        Types::write(output, val);
    }
    void operator()(bsl::int8_t val) const
    {
        Types::write(output, FieldValue::SHORT_SHORT_INT);
        Types::write(output, val);
    }
    void operator()(bsl::int16_t val) const
    {
        Types::write(output, FieldValue::SHORT_INT);
        Types::write(output, bdlb::BigEndianInt16::make(val));
    }
    void operator()(bsl::uint16_t val) const
    {
        Types::write(output, FieldValue::SHORT_UINT);
        Types::write(output, bdlb::BigEndianUint16::make(val));
    }
    void operator()(bsl::int32_t val) const
    {
        Types::write(output, FieldValue::LONG_INT);
        Types::write(output, bdlb::BigEndianInt32::make(val));
    }
    void operator()(bsl::uint32_t val) const
    {
        Types::write(output, FieldValue::LONG_UINT);
        Types::write(output, bdlb::BigEndianUint32::make(val));
    }
    void operator()(bsl::int64_t val) const
    {
        Types::write(output, FieldValue::RABBIT_SPEC_LONG_LONG_INT);
        Types::write(output, bdlb::BigEndianInt64::make(val));
    }
    void operator()(bsl::uint64_t val) const
    {
        // unsigned long long field type is not supported inside field table by
        // RabbitMQ: https://www.rabbitmq.com/amqp-0-9-1-errata.html
        // But for backward compatible changes we are still going to encode this
        // as unsigned long long int. And decoding will always be signed long
        // long int.
        Types::write(output, FieldValue::RABBIT_SPEC_LONG_LONG_INT);
        Types::write(output, bdlb::BigEndianUint64::make(val));
    }
    void operator()(float val) const
    {
        bsl::uint32_t temp;
        bsl::memcpy(&temp, &val, sizeof(val));

        Types::write(output, FieldValue::FLOAT);
        Types::write(output, bdlb::BigEndianUint32::make(temp));
    }
    void operator()(double val) const
    {
        bsl::uint64_t temp;
        bsl::memcpy(&temp, &val, sizeof(val));

        Types::write(output, FieldValue::DOUBLE);
        Types::write(output, bdlb::BigEndianUint64::make(temp));
    }
    void operator()(const rmqt::ShortString& str) const
    {
        // short string field type is not supported inside field table by
        // RabbitMQ. So we will encode a short string as a long string:
        // https://www.rabbitmq.com/amqp-0-9-1-errata.html
        Types::write(output, FieldValue::LONG_STRING);
        Types::encodeLongString(output, str);
    }
    void operator()(const bsl::string& str) const
    {
        Types::write(output, FieldValue::LONG_STRING);
        Types::encodeLongString(output, str);
    }
    void operator()(const bsl::vector<bsl::uint8_t>& array) const
    {
        Types::write(output, FieldValue::BYTE_ARRAY);
        Types::write(output, bdlb::BigEndianUint32::make(array.size()));
        Types::encodeByteVector(output, array);
    }
    void operator()(const bsl::shared_ptr<rmqt::FieldArray>& array) const
    {
        Types::write(output, FieldValue::FIELD_ARRAY);
        Types::encodeFieldArray(output, *array);
    }
    void operator()(const bsl::shared_ptr<rmqt::FieldTable>& table) const
    {
        Types::write(output, FieldValue::FIELD_TABLE);
        Types::encodeFieldTable(output, *table);
    }
    void operator()(const bdlt::Datetime& dt) const
    {
        Types::write(output, FieldValue::TIMESTAMP);
        Types::encodeTimestamp(output, dt);
    }
    void operator()(const bslmf::Nil&) const
    {
        Types::write(output, FieldValue::NO_FIELD);
    }
};

void Types::encodeFieldValue(Writer& output, const rmqt::FieldValue& fv)
{
    fv.apply(FieldValueEncoder(output));
}

bool Types::decodeFieldArray(rmqt::FieldArray* fieldArray,
                             rmqamqpt::Buffer* buffer)
{
    BSLS_ASSERT(fieldArray);

    const bdlb::BigEndianUint32 arrayLength =
        buffer->copy<bdlb::BigEndianUint32>();
    if (arrayLength > buffer->available()) {
        return false;
    }

    rmqamqpt::Buffer arrayBuffer = buffer->consume(arrayLength);
    while (arrayBuffer.available() > 0) {
        rmqt::FieldValue value;
        if (!decodeFieldValue(&value, &arrayBuffer)) {
            return false;
        }

        fieldArray->push_back(value);
    }

    return true;
}

void Types::encodeFieldArray(Writer& output, const rmqt::FieldArray& v)
{
    const bsl::size_t hostOrderLen =
        rmqamqpt::FieldValueUtil::fieldArrayContentsEncodedSize(v);

    Types::write(output, bdlb::BigEndianUint32::make(hostOrderLen));

    const bsl::size_t numItems = v.size();
    for (bsl::size_t i = 0; i < numItems; ++i) {
        encodeFieldValue(output, v[i]);
    }
}

bool Types::decodeFieldTable(rmqt::FieldTable* table, rmqamqpt::Buffer* buffer)
{
    const bdlb::BigEndianUint32 fieldTableLength =
        buffer->copy<bdlb::BigEndianUint32>();
    if (fieldTableLength > buffer->available()) {
        BALL_LOG_ERROR << "Not enough data to read table size: "
                       << fieldTableLength
                       << " available: " << buffer->available();
        return false;
    }

    rmqamqpt::Buffer tBuffer = buffer->consume(fieldTableLength);
    while (tBuffer.available() > 0) {
        bsl::string fieldName;
        if (!decodeShortString(&fieldName, &tBuffer)) {
            BALL_LOG_ERROR << "Cannot read Field name";
            return false;
        }

        rmqt::FieldValue value;
        if (!decodeFieldValue(&value, &tBuffer)) {
            BALL_LOG_ERROR << "Cannot read Field value: " << fieldName;
            return false;
        }

        const bsl::pair<rmqt::FieldTable::iterator, bool> result =
            table->insert(bsl::make_pair(fieldName, value));

        if (!result.second) {
            BALL_LOG_ERROR << "Duplicate FieldTable key [" << fieldName
                           << "]. Using value [" << result.first->second
                           << "], dropping [" << value << "]";
        }
    }
    return true;
}

void Types::encodeFieldTable(Writer& output, const rmqt::FieldTable& table)
{
    Types::write(
        output,
        bdlb::BigEndianUint32::make(FieldValueUtil::encodedTableSize(table)));

    for (rmqt::FieldTable::const_iterator it = table.begin(); it != table.end();
         ++it) {
        encodeShortString(output, it->first);
        encodeFieldValue(output, it->second);
    }
}

void Types::encodeTimestamp(Writer& output, const bdlt::Datetime& timestamp)

{
    Types::write(output,
                 bdlb::BigEndianInt64::make(
                     bdlt::EpochUtil::convertToTimeT64(timestamp)));
}

void Types::decodeTimestamp(bdlt::Datetime* timestamp, rmqamqpt::Buffer* buffer)
{
    *timestamp = bdlt::EpochUtil::convertFromTimeT64(
        buffer->copy<bdlb::BigEndianInt64>());
}

} // namespace rmqamqpt
} // namespace BloombergLP
