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

#ifndef INCLUDED_RMQAMQPT_TYPES
#define INCLUDED_RMQAMQPT_TYPES

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>

#include <bsl_cstdint.h>
#include <bsl_sstream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

#include <bdlb_bigendian.h>
#include <bsl_cstddef.h>
#include <bsl_type_traits.h>

namespace BloombergLP {
namespace rmqamqpt {

class Types {
  public:
    // https://www.rabbitmq.com/amqp-0-9-1-reference.html#domain.delivery-tag
    typedef bsl::uint64_t DeliveryTag;

    typedef bsl::string ConsumerTag;

    static bool decodeLongString(bsl::string* string, rmqamqpt::Buffer* buffer);

    static bool decodeShortString(bsl::string* string,
                                  rmqamqpt::Buffer* buffer);

    static void encodeLongString(Writer& output, const bsl::string& string);

    static void encodeShortString(Writer& output, const bsl::string& string);

    static bool decodeByteVector(bsl::vector<bsl::uint8_t>* vector,
                                 rmqamqpt::Buffer* buffer,
                                 bsl::size_t bytes);

    static void encodeByteVector(Writer& output,
                                 const bsl::vector<bsl::uint8_t>& data);

    static bool decodeFieldValue(rmqt::FieldValue* value,
                                 rmqamqpt::Buffer* buffer);

    static void encodeFieldValue(Writer& output, const rmqt::FieldValue& value);

    static bool decodeFieldArray(rmqt::FieldArray* fieldArray,
                                 rmqamqpt::Buffer* buffer);

    static void encodeFieldArray(Writer& output, const rmqt::FieldArray& data);

    static bool decodeFieldTable(rmqt::FieldTable* table,
                                 rmqamqpt::Buffer* buffer);

    static void encodeFieldTable(Writer& output, const rmqt::FieldTable& table);

    static void encodeTimestamp(Writer& output,
                                const bdlt::Datetime& timestamp);

    static void decodeTimestamp(bdlt::Datetime* timestamp,
                                rmqamqpt::Buffer* buffer);

    template <typename T>
    static void write(Writer& output, const T& num)
    {
#ifdef BSLS_COMPILERFEATURES_SUPPORT_STATIC_ASSERT
        // Ensure the bytes are always written in big-endian order
        static_assert(bsl::is_same<T, bool>::value ||
                          bsl::is_same<T, signed char>::value ||
                          bsl::is_same<T, unsigned char>::value ||
                          bsl::is_same<T, bdlb::BigEndianInt16>::value ||
                          bsl::is_same<T, bdlb::BigEndianInt32>::value ||
                          bsl::is_same<T, bdlb::BigEndianInt64>::value ||
                          bsl::is_same<T, bdlb::BigEndianUint16>::value ||
                          bsl::is_same<T, bdlb::BigEndianUint32>::value ||
                          bsl::is_same<T, bdlb::BigEndianUint64>::value,
                      "Unsupported type");
#endif
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&num);
        output.write(data, sizeof(num));
    }

    static void write(Writer& output, FieldValue::Type fieldType)
    {
        const uint8_t data = static_cast<uint8_t>(fieldType);
        output.write(&data, sizeof(data));
    }
};

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
