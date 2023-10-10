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

#ifndef INCLUDED_RMQT_EXCHANGETYPE
#define INCLUDED_RMQT_EXCHANGETYPE
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief AMQP Exchange types
///
/// This class specifies the different AMQP exchange types.

class ExchangeType {
  public:
    /// When using plugin-based exchange types
    explicit ExchangeType(const bsl::string&);
    const bsl::string& type() const;
    static ExchangeType DIRECT;
    static ExchangeType FANOUT;
    static ExchangeType TOPIC;
    static ExchangeType HEADERS;

  private:
    const bsl::string d_type;
};

bool operator==(const ExchangeType& lhs, const ExchangeType& rhs);
bool operator!=(const ExchangeType& lhs, const ExchangeType& rhs);

} // namespace rmqt
} // namespace BloombergLP

#endif
