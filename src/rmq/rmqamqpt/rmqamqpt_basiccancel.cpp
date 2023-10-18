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

#include <rmqamqpt_basiccancel.h>
#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bsl_cstddef.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqamqpt {

BasicCancel::BasicCancel(const bsl::string& consumerTag, bool noWait)
: d_consumerTag(consumerTag)
, d_noWait(noWait)
{
}
BasicCancel::BasicCancel()
: d_consumerTag()
, d_noWait()
{
}

bool BasicCancel::decode(BasicCancel* cancel,
                         const uint8_t* data,
                         bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);
    if (!Types::decodeShortString(&cancel->d_consumerTag, &buffer)) {
        return false;
    }
    if (buffer.available() < sizeof(bool)) {
        return false;
    }
    cancel->d_noWait = buffer.copy<bool>();

    return true;
}

void BasicCancel::encode(Writer& output, const BasicCancel& cancel)
{
    Types::encodeShortString(output, cancel.d_consumerTag);
    Types::write(output, cancel.d_noWait);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicCancel& cancel)
{
    return os << "BasicCancel: [consumer-tag: " << cancel.consumerTag()
              << ", no-wait: " << cancel.noWait() << "]";
}

bool operator==(const BasicCancel& lhs, const BasicCancel& rhs)
{
    return (&lhs == &rhs) || (lhs.consumerTag() == rhs.consumerTag() &&
                              lhs.noWait() == rhs.noWait());
}

} // namespace rmqamqpt
} // namespace BloombergLP
