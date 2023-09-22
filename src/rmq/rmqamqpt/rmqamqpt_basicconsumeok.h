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

#ifndef INCLUDED_RMQAMQPT_BASICCONSUMEOK
#define INCLUDED_RMQAMQPT_BASICCONSUMEOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_types.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide CONSUME-OK method
/// The server provides the client with a consumer tag, which is used by the
/// client for methods called on the consumer at a later stage.

class BasicConsumeOk {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_CONSUMEOK;

    explicit BasicConsumeOk(const bsl::string& consumerTag);
    BasicConsumeOk();

    size_t encodedSize() const
    {
        return sizeof(uint8_t) + d_consumerTag.size();
    }

    const bsl::string& consumerTag() const { return d_consumerTag; }

    static bool decode(BasicConsumeOk*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicConsumeOk&);

  private:
    Types::ConsumerTag d_consumerTag;
};

bsl::ostream& operator<<(bsl::ostream& os,
                         const BasicConsumeOk& consumeOkMethod);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
