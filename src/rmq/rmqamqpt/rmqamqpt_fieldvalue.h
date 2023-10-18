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

#ifndef INCLUDED_RMQAMQPT_FIELDVALUE
#define INCLUDED_RMQAMQPT_FIELDVALUE

#include <bsl_cstddef.h>
#include <rmqt_fieldvalue.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief A Variant representing AMQP 0.9.1 field types inside the field table.
/// But there are some differences between RabbitMQ implementation and actual
/// AMQP 0.9.1 protocol for encoding field types. Specified in details here:
/// https://www.rabbitmq.com/amqp-0-9-1-errata.html#section_3
class FieldValue {
  public:
    enum Type {
        BOOLEAN                   = 't',
        SHORT_SHORT_INT           = 'b',
        SHORT_SHORT_UINT          = 'B',
        SHORT_INT                 = 's',
        SHORT_UINT                = 'u',
        LONG_INT                  = 'I',
        LONG_UINT                 = 'i',
        AMQP_SPEC_LONG_LONG_INT   = 'L',
        RABBIT_SPEC_LONG_LONG_INT = 'l',
        FLOAT                     = 'f',
        DOUBLE                    = 'd',
        DECIMAL                   = 'D',
        LONG_STRING               = 'S',
        BYTE_ARRAY                = 'x',
        FIELD_ARRAY               = 'A',
        TIMESTAMP                 = 'T',
        FIELD_TABLE               = 'F',
        NO_FIELD                  = 'V'
    };
};

struct FieldValueUtil {
    static bsl::size_t encodedSize(const rmqt::FieldValue& fv);
    static bsl::size_t encodedValueSize(const rmqt::FieldValue& fv);

    static bsl::size_t encodedTableSize(const rmqt::FieldTable& table);

    static bsl::size_t
    fieldArrayContentsEncodedSize(const rmqt::FieldArray& fieldArray);
};

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
