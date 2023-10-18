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

#ifndef INCLUDED_RMQAMQPT_QUEUEDECLAREOK
#define INCLUDED_RMQAMQPT_QUEUEDECLAREOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief  Provide queue DECLARE-OK method
///
/// This method confirms a Declare method and confirms the name of the queue,
/// essential for automatically-named queues.

class QueueDeclareOk {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::QUEUE_DECLAREOK;

    QueueDeclareOk();

    QueueDeclareOk(const bsl::string& name,
                   bsl::uint32_t messageCount,
                   bsl::uint32_t consumerCount);

    size_t encodedSize() const
    {
        return sizeof(uint8_t) + 2 * sizeof(uint32_t) + d_name.size();
    }

    const bsl::string& name() const { return d_name; }

    bsl::uint32_t messageCount() const { return d_messageCount; }

    bsl::uint32_t consumerCount() const { return d_consumerCount; }

    static bool decode(QueueDeclareOk* declare,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const QueueDeclareOk& declare);

  private:
    bsl::string d_name;
    bsl::uint32_t d_messageCount;
    bsl::uint32_t d_consumerCount;
};

bool operator==(const QueueDeclareOk& lhs, const QueueDeclareOk& rhs);

inline bool operator!=(const QueueDeclareOk& lhs, const QueueDeclareOk& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const QueueDeclareOk& queueDeclareOk);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
