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

#include <rmqamqpt_basiccancelok.h>

#include <rmqamqpt_types.h>

#include <rmqamqpt_buffer.h>

#include <bsl_cstddef.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqamqpt {

BasicCancelOk::BasicCancelOk(const bsl::string& consumerTag)
: d_consumerTag(consumerTag)
{
}

BasicCancelOk::BasicCancelOk()
: d_consumerTag()
{
}

bool BasicCancelOk::decode(BasicCancelOk* cancelok,
                           const uint8_t* data,
                           bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);
    return Types::decodeShortString(&cancelok->d_consumerTag, &buffer);
}

void BasicCancelOk::encode(Writer& output, const BasicCancelOk& cancelok)
{
    Types::encodeShortString(output, cancelok.d_consumerTag);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicCancelOk& cancelok)
{
    return os << "BasicCancelOk: [consumer-tag: " << cancelok.consumerTag()
              << "]";
}

} // namespace rmqamqpt
} // namespace BloombergLP
