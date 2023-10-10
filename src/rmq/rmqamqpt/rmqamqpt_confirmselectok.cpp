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

#include <rmqamqpt_confirmselectok.h>

#include <bsl_cstddef.h>
#include <rmqamqpt_types.h>

namespace BloombergLP {
namespace rmqamqpt {

bool ConfirmSelectOk::decode(ConfirmSelectOk*, const uint8_t*, bsl::size_t)
{
    // nothing to decode
    return true;
}

void ConfirmSelectOk::encode(Writer&, const ConfirmSelectOk&) {}

bsl::ostream& operator<<(bsl::ostream& os, const ConfirmSelectOk&)
{
    os << "Confirm Select = []";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
