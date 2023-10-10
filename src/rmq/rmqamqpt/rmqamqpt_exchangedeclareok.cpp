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

#include <rmqamqpt_exchangedeclareok.h>

#include <bsl_ostream.h>
#include <bsl_sstream.h>

namespace BloombergLP {
namespace rmqamqpt {

ExchangeDeclareOk::ExchangeDeclareOk() {}

bool ExchangeDeclareOk::decode(ExchangeDeclareOk*, const uint8_t*, bsl::size_t)
{
    // nothing to decode
    return true;
}

void ExchangeDeclareOk::encode(Writer&, const ExchangeDeclareOk&) {}

bool operator==(const ExchangeDeclareOk&, const ExchangeDeclareOk&)
{
    return true;
}

bsl::ostream& operator<<(bsl::ostream& os, const ExchangeDeclareOk&)
{
    os << "ExchangeDeclareOk = []";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
