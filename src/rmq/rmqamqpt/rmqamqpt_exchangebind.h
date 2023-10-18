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

#ifndef INCLUDED_RMQAMQPT_EXCHANGEBIND
#define INCLUDED_RMQAMQPT_EXCHANGEBIND

#include <rmqamqpt_constants.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide exchange BIND method
///
/// This method binds an exchange to an exchange.

class ExchangeBind {
  public:
    static const int METHOD_ID = rmqamqpt::Constants::EXCHANGE_BIND;

    ExchangeBind();

    ExchangeBind(const bsl::string& sourceX,
                 const bsl::string& destinationX,
                 const bsl::string& routingKey,
                 bool noWait,
                 const rmqt::FieldTable& arguments);

    bsl::size_t encodedSize() const
    {
        return sizeof(uint16_t) + 4 * sizeof(uint8_t) + sizeof(uint32_t) +
               d_sourceX.size() + d_destinationX.size() + d_routingKey.size() +
               FieldValueUtil::encodedTableSize(d_arguments);
    }

    const bsl::string& sourceX() const { return d_sourceX; }

    const bsl::string& destinationX() const { return d_destinationX; }

    const bsl::string& routingKey() const { return d_routingKey; }

    bool noWait() const { return d_noWait; }

    const rmqt::FieldTable& arguments() const { return d_arguments; }

    static bool
    decode(ExchangeBind* bind, const uint8_t* data, bsl::size_t dataLength);
    static void encode(Writer& output, const ExchangeBind& bind);

  private:
    bsl::string d_sourceX;
    bsl::string d_destinationX;
    bsl::string d_routingKey;
    bool d_noWait;
    rmqt::FieldTable d_arguments;
};

bool operator==(const ExchangeBind& lhs, const ExchangeBind& rhs);

inline bool operator!=(const ExchangeBind& lhs, const ExchangeBind& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os, const ExchangeBind& exchangeBind);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
