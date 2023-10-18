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

#include <rmqamqpt_fieldvalue.h>

#include <rmqamqpt_constants.h>

#include <bdlt_epochutil.h>
#include <bsl_cstddef.h>
#include <bsl_limits.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

BSLMF_ASSERT(sizeof(bsl::int8_t) == 1);
BSLMF_ASSERT(sizeof(bsl::int16_t) == 2);
BSLMF_ASSERT(sizeof(bsl::int32_t) == 4);
BSLMF_ASSERT(sizeof(bsl::uint8_t) == 1);
BSLMF_ASSERT(sizeof(bsl::uint16_t) == 2);
BSLMF_ASSERT(sizeof(bsl::uint32_t) == 4);
BSLMF_ASSERT(sizeof(float) == 4);
BSLMF_ASSERT(sizeof(double) == 8);
BSLMF_ASSERT(sizeof(bsls::Types::Int64) == 8);
BSLMF_ASSERT(sizeof(bdlt::EpochUtil::TimeT64) == 8);

#if BSLS_COMPILERFEATURES_CPLUSPLUS >= 201103L
BSLMF_ASSERT(bsl::numeric_limits<float>::is_iec559);
BSLMF_ASSERT(bsl::numeric_limits<double>::is_iec559);
#endif

bsl::size_t FieldValueUtil::encodedSize(const rmqt::FieldValue& fv)
{
    const bsl::size_t typeSize = sizeof(bsl::uint8_t);

    return typeSize + encodedValueSize(fv);
}

bsl::size_t FieldValueUtil::fieldArrayContentsEncodedSize(
    const rmqt::FieldArray& fieldArray)
{
    bsl::size_t size = 0;
    for (rmqt::FieldArray::const_iterator it = fieldArray.begin();
         it != fieldArray.end();
         ++it) {
        size += encodedSize(*it);
    }

    return size;
}

class FVSizeCalculator {
  public:
    typedef bsl::size_t ResultType;

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bool value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bsl::int8_t value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bsl::int16_t value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bsl::int32_t value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bsl::int64_t value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bsl::uint8_t value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bsl::uint16_t value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bsl::uint32_t value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(bsl::uint64_t value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(float value) const
    {
        return sizeof(value);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(double value) const
    {
        return sizeof(value);
    }

    ResultType operator()(const bsl::string& s) const
    {
        return sizeof(bsl::uint32_t) + s.length();
    }

    ResultType operator()(const bsl::vector<bsl::uint8_t>& value) const
    {
        return sizeof(bsl::uint32_t) + value.size();
    }

    ResultType operator()(const bsl::shared_ptr<rmqt::FieldArray>& arr) const
    {
        const ResultType arrayLengthSize = sizeof(uint32_t);
        return arrayLengthSize +
               FieldValueUtil::fieldArrayContentsEncodedSize(*arr);
    }

    ResultType operator()(const bsl::shared_ptr<rmqt::FieldTable>& table) const
    {
        const ResultType tableLengthSize = sizeof(uint32_t);
        return tableLengthSize + FieldValueUtil::encodedTableSize(*table);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(const bdlt::Datetime&) const
    {
        return sizeof(bdlt::EpochUtil::TimeT64);
    }

    BSLS_KEYWORD_CONSTEXPR ResultType operator()(const bslmf::Nil&) const
    {
        return 0;
    }
};

bsl::size_t FieldValueUtil::encodedValueSize(const rmqt::FieldValue& fv)
{
    return fv.apply(FVSizeCalculator());
}

bsl::size_t FieldValueUtil::encodedTableSize(const rmqt::FieldTable& table)
{
    bsl::size_t size = 0;
    for (rmqt::FieldTable::const_iterator it = table.cbegin();
         it != table.cend();
         ++it) {

        size += sizeof(bsl::uint8_t) + it->first.size();
        size += encodedSize(it->second);
    }
    return size;
}

} // namespace rmqamqpt
} // namespace BloombergLP
