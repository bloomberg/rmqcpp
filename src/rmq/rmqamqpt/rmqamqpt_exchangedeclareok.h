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

#ifndef INCLUDED_RMQAMQPT_EXCHANGEDECLAREOK
#define INCLUDED_RMQAMQPT_EXCHANGEDECLAREOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bdlb_bigendian.h>
#include <bsl_cstdlib.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide exchange DECLARE-OK method
///
/// This method confirms a Declare method and confirms the name of the exchange,
/// essential for automatically-named exchanges.

class ExchangeDeclareOk {
  public:
    static const int METHOD_ID = rmqamqpt::Constants::EXCHANGE_DECLAREOK;

    ExchangeDeclareOk();

    size_t encodedSize() const { return 0; }

    static bool decode(ExchangeDeclareOk* declare,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const ExchangeDeclareOk& declare);
};

bool operator==(const ExchangeDeclareOk& lhs, const ExchangeDeclareOk& rhs);

inline bool operator!=(const ExchangeDeclareOk& lhs,
                       const ExchangeDeclareOk& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const ExchangeDeclareOk& exchangeDeclareOk);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
