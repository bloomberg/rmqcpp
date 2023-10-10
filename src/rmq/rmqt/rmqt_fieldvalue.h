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

#ifndef INCLUDED_RMQT_FIELDVALUE
#define INCLUDED_RMQT_FIELDVALUE

#include <rmqt_shortstring.h>

#include <bdlb_variant.h>
#include <bdlt_datetime.h>
#include <bsls_compilerfeatures.h>

#include <bsl_cstdint.h>
#include <bsl_map.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_string.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

#ifdef BSLS_COMPILERFEATURES_SUPPORT_GENERALIZED_INITIALIZERS
#include <bsl_initializer_list.h>
#endif

namespace BloombergLP {
namespace rmqt {

struct FieldArray;
struct FieldTable;

/// \brief A Variant representing AMQP 0.9.1 typed field values
///
/// The type will affect how the value is serialized when sent to the broker.
/// Care must be taken around integers and strings.
///
/// There are a number of different integer sizes, and constructing a
/// `FieldValue(5000000)` may produce different results on different platforms
/// Where possible, use the precise, sized integer type such as `uint32_t`
///
/// unsigned long long int(uint64_t) is not supported as a field type inside
/// field table. Internally, uint64_t will be converted into int64_t by library.
/// So if the number overflow the max limit, then the behaviour will be
/// undefined.
///
/// There are two types of string in AMQP 0.9.1: Short String and
/// Long String:
///    - A ShortString must have 255 or fewer bytes. But explicit ShortString is
///    not supported as a field type inside field table by RabbitMQ. So if
///    anyone wants to use short string inside the field table, they can use
///    long string as an alternative.
///    - A LongString (bsl::string) can contain up to `2^32 - 1` bytes when
///    serialized
///
/// https://www.rabbitmq.com/amqp-0-9-1-errata.html
///
/// TODO: Eventually, we are going to delete uint64_t support as field type
/// inside field table. So avoid using this type, instead use int64_t.
typedef bdlb::Variant<bool,
                      int8_t,
                      uint8_t,
                      int16_t,
                      uint16_t,
                      int32_t,
                      uint32_t,
                      int64_t,
                      uint64_t,
                      float,
                      double,
                      bsl::string,
                      bsl::vector<bsl::uint8_t>,
                      bsl::shared_ptr<FieldArray>,
                      bdlt::Datetime,
                      bsl::shared_ptr<FieldTable> >
    FieldValue;

/// \brief Represents AMQP 0.9.1 `Field Array` (list of FieldValues)
///
/// Once constructed, a FieldArray should be treated as a `vector<FieldValue>`
struct FieldArray : public bsl::vector<FieldValue> {
    FieldArray();
    explicit FieldArray(const bsl::vector<FieldValue>& array);

#ifdef BSLS_COMPILERFEATURES_SUPPORT_GENERALIZED_INITIALIZERS
    FieldArray(bsl::initializer_list<FieldValue> items);

    static bsl::shared_ptr<FieldArray> make(bsl::initializer_list<FieldValue>);
#endif
};

/// \brief Represents AMQP 0.9.1 `Field Table` (dict of FieldValues)
///
/// Once constructed, a FieldTable should behave/be treated like a
/// `map<string, FieldValue>`.
/// The table keys will be truncated when serialised to AMQP if they are more
/// than 255 characters long
struct FieldTable : public bsl::map<bsl::string, FieldValue> {

    FieldTable();
    explicit FieldTable(const bsl::map<bsl::string, FieldValue>& t);

#ifdef BSLS_COMPILERFEATURES_SUPPORT_GENERALIZED_INITIALIZERS
    FieldTable(
        bsl::initializer_list<bsl::pair<const bsl::string, FieldValue> > items);

    static bsl::shared_ptr<FieldTable>
        make(bsl::initializer_list<bsl::pair<const bsl::string, FieldValue> >);
#endif

    bsl::ostream&
    print(bsl::ostream& stream, int level, int spacesPerLevel) const;
};

bool operator==(const rmqt::FieldValue& left, const rmqt::FieldValue& right);
bool operator!=(const rmqt::FieldValue& left, const rmqt::FieldValue& right);

bsl::ostream& operator<<(bsl::ostream& os, const FieldTable& table);
bsl::ostream& operator<<(bsl::ostream& os,
                         const bsl::shared_ptr<FieldTable>& table);
bsl::ostream& operator<<(bsl::ostream& os, const FieldValue& value);

} // namespace rmqt
} // namespace BloombergLP

#endif
