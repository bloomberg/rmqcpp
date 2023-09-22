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

#include <rmqamqpt_basicconsumeok.h>

#include <bsl_cstddef.h>
#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

namespace BloombergLP {
namespace rmqamqpt {

BasicConsumeOk::BasicConsumeOk(const bsl::string& consumerTag)
: d_consumerTag(consumerTag)
{
}

BasicConsumeOk::BasicConsumeOk()
: d_consumerTag()
{
}

bool BasicConsumeOk::decode(BasicConsumeOk* consumeOk,
                            const uint8_t* data,
                            bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    return Types::decodeShortString(&consumeOk->d_consumerTag, &buffer);
}

void BasicConsumeOk::encode(Writer& output, const BasicConsumeOk& consumeOk)
{
    Types::encodeShortString(output, consumeOk.d_consumerTag);
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const BasicConsumeOk& consumeOkMethod)
{
    return os << "BasicConsumeOk: [consumer-tag: "
              << consumeOkMethod.consumerTag() << "]";
}

} // namespace rmqamqpt
} // namespace BloombergLP
