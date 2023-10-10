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

#ifndef INCLUDED_RMQAMQPT_EXCHANGEDECLARE
#define INCLUDED_RMQAMQPT_EXCHANGEDECLARE

#include <rmqamqpt_constants.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide exchange DECLARE method
///
/// This method creates an exchange if it does not already exist, and if the
/// exchange exists, verifies that it is of the correct and expected class.

class ExchangeDeclare {
  public:
    static const int METHOD_ID = rmqamqpt::Constants::EXCHANGE_DECLARE;

    ExchangeDeclare();

    ExchangeDeclare(const bsl::string& name,
                    const bsl::string& type,
                    bool passive,
                    bool durable,
                    bool autoDelete,
                    bool internal,
                    bool noWait,
                    const rmqt::FieldTable& arguments);

    bsl::size_t encodedSize() const
    {
        return sizeof(uint16_t) + 3 * sizeof(uint8_t) + sizeof(uint32_t) +
               d_name.size() + d_type.size() +
               FieldValueUtil::encodedTableSize(d_arguments);
    }

    const bsl::string& name() const { return d_name; }

    const bsl::string& type() const { return d_type; }

    bool passive() const { return d_passive; }

    bool durable() const { return d_durable; }

    bool autoDelete() const { return d_autoDelete; }

    bool internal() const { return d_internal; }

    bool noWait() const { return d_noWait; }

    const rmqt::FieldTable& arguments() const { return d_arguments; }

    static bool decode(ExchangeDeclare* declare,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const ExchangeDeclare& declare);

  private:
    bsl::string d_name;
    bsl::string d_type;
    bool d_passive;
    bool d_durable;
    bool d_autoDelete;
    bool d_internal;
    bool d_noWait;
    rmqt::FieldTable d_arguments;
};

bool operator==(const ExchangeDeclare& lhs, const ExchangeDeclare& rhs);

inline bool operator!=(const ExchangeDeclare& lhs, const ExchangeDeclare& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const ExchangeDeclare& exchangeDeclare);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
