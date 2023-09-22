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

#include <rmqamqpt_confirmselect.h>
#include <rmqamqpt_types.h>

#include <bsl_cstddef.h>
#include <rmqamqpt_buffer.h>

namespace BloombergLP {
namespace rmqamqpt {

ConfirmSelect::ConfirmSelect()
: d_noWait()
{
}

ConfirmSelect::ConfirmSelect(bool no_wait)
: d_noWait(no_wait)
{
}

bool ConfirmSelect::decode(ConfirmSelect* select,
                           const uint8_t* data,
                           bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);
    uint8_t bitmask  = buffer.copy<uint8_t>();
    select->d_noWait = bitmask & 0x1;
    return true;
}

void ConfirmSelect::encode(Writer& output, const ConfirmSelect& select)
{
    Types::write(output, static_cast<uint8_t>(select.d_noWait));
}

bsl::ostream& operator<<(bsl::ostream& os, const ConfirmSelect& confirmSelect)
{
    os << "Confirm Select = [no-wait: " << confirmSelect.noWait() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
