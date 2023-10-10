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

#include <rmqt_fieldvalue.h>

#include <bsls_compilerfeatures.h>

#include <bsl_cstddef.h>
#include <bsl_ios.h>
#include <bsl_map.h>
#include <bsl_ostream.h>
#include <bsl_string.h>
#include <bsl_utility.h>

#ifdef BSLS_COMPILERFEATURES_SUPPORT_GENERALIZED_INITIALIZERS
#include <bsl_initializer_list.h>
#endif

namespace BloombergLP {
namespace rmqt {
namespace {

class FieldValuePrinter {
    bsl::ostream& d_printStream;

  public:
    explicit FieldValuePrinter(bsl::ostream& stream)
    : d_printStream(stream)
    {
    }

    template <typename T>
    void operator()(T value) const
    {
        d_printStream << value;
    }

    // Print uint8_t as an int, not a char
    void operator()(bsl::uint8_t value) const { d_printStream << int(value); }

    // Print bool as true/false, not 1/0
    void operator()(bool value) const
    {
        d_printStream << bsl::boolalpha << value << bsl::noboolalpha;
    }

    void operator()(const bsl::shared_ptr<rmqt::FieldArray>& value) const
    {
        d_printStream << "[";
        for (bsl::size_t i = 0, l = value->size(); i < l; ++i) {
            d_printStream << (*value)[i];
            if (i < l - 1) {
                d_printStream << ", ";
            }
        }
        d_printStream << "]";
    }

    void operator()(const bsl::shared_ptr<rmqt::FieldTable>& value) const
    {
        d_printStream << *value;
    }

    void operator()(const bsl::string& value) const
    {
        d_printStream << "\"" << value << "\"";
    }

    void operator()(const bsl::vector<bsl::uint8_t>& value) const
    {
        d_printStream << "[";
        for (bsl::vector<bsl::uint8_t>::const_iterator i   = value.cbegin(),
                                                       end = value.cend();
             i != end;
             ++i) {
            if (i != value.cbegin()) {
                d_printStream << ", ";
            }
            d_printStream << int(*i);
        }
        d_printStream << "]";
    }

    void operator()(const bslmf::Nil&) const { d_printStream << "<Nil>"; }
};
} // namespace

FieldArray::FieldArray() {}

FieldArray::FieldArray(const bsl::vector<FieldValue>& items)
: bsl::vector<FieldValue>(items)
{
}

FieldTable::FieldTable() {}

FieldTable::FieldTable(const bsl::map<bsl::string, FieldValue>& items)
: bsl::map<bsl::string, FieldValue>(items)
{
}

class FieldValueEquality {
    const FieldValue d_left;

  public:
    typedef bool ResultType;

    explicit FieldValueEquality(const FieldValue& left)
    : d_left(left)
    {
    }

    template <typename T>
    bool operator()(const bsl::shared_ptr<T>& ptr) const
    {
        if (!d_left.is<bsl::shared_ptr<T> >()) {
            return false;
        }

        const bsl::shared_ptr<T>& leftFt = d_left.the<bsl::shared_ptr<T> >();

        if (!leftFt || !ptr) {
            return leftFt == ptr;
        }

        return *leftFt == *ptr;
    }

    template <typename T>
    bool operator()(const T& val) const
    {
        if (!d_left.is<T>()) {
            return false;
        }

        return d_left.the<T>() == val;
    }

    bool operator()(const bslmf::Nil&) const { return d_left.isUnset(); }
};

bool operator==(const rmqt::FieldValue& left, const rmqt::FieldValue& right)
{
    return right.apply(FieldValueEquality(left));
}

bool operator!=(const rmqt::FieldValue& left, const rmqt::FieldValue& right)
{
    return !(left == right);
}

bsl::ostream& operator<<(bsl::ostream& os, const FieldTable& table)
{
    return table.print(os, 0, -1);
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const bsl::shared_ptr<FieldTable>& table)
{
    if (table) {
        return os << *table;
    }
    return os << "null";
}

bsl::ostream& operator<<(bsl::ostream& os, const FieldValue& value)
{
    value.apply(FieldValuePrinter(os));
    return os;
}

#ifdef BSLS_COMPILERFEATURES_SUPPORT_GENERALIZED_INITIALIZERS

FieldArray::FieldArray(bsl::initializer_list<FieldValue> items)
: bsl::vector<FieldValue>(items)
{
}

FieldTable::FieldTable(
    bsl::initializer_list<bsl::pair<const bsl::string, FieldValue> > items)
{
    this->insert(items);
}

bsl::shared_ptr<FieldArray>
FieldArray::make(bsl::initializer_list<FieldValue> list)
{
    return bsl::make_shared<FieldArray>(list);
}

bsl::shared_ptr<FieldTable> FieldTable::make(
    bsl::initializer_list<bsl::pair<const bsl::string, FieldValue> > table)
{
    return bsl::make_shared<FieldTable>(table);
}

#endif

bsl::ostream& FieldTable::print(bsl::ostream& stream,
                                BSLA_MAYBE_UNUSED int level,
                                BSLA_MAYBE_UNUSED int spacesPerLevel) const
{
    stream << " [";
    for (FieldTable::const_iterator it = cbegin(); it != cend(); ++it) {
        if (it != cbegin()) {
            stream << ",";
        }

        stream << " " << it->first << " = " << it->second;
    }
    stream << " ]";
    return stream;
}

} // namespace rmqt
} // namespace BloombergLP
