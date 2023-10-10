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

#ifndef INCLUDED_RMQAMQPT_BASICCANCELOK
#define INCLUDED_RMQAMQPT_BASICCANCELOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

class BasicCancelOk {
    // This method confirms that the cancellation was completed.
  private:
    bsl::string d_consumerTag;

  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_CANCELOK;

    explicit BasicCancelOk(const bsl::string& consumerTag);
    BasicCancelOk();

    size_t encodedSize() const
    {
        return sizeof(uint8_t) + d_consumerTag.size();
    }

    // Identifier for the consumer, valid within the current channel.
    const bsl::string& consumerTag() const { return d_consumerTag; }

    static bool decode(BasicCancelOk*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicCancelOk&);
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicCancelOk&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
