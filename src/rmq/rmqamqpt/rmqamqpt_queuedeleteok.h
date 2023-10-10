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

#ifndef INCLUDED_RMQAMQPT_QUEUEDELETEOK
#define INCLUDED_RMQAMQPT_QUEUEDELETEOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide queue DELETE-OK method
///
/// This method confirms the deletion of a queue.

class QueueDeleteOk {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::QUEUE_DELETEOK;

    QueueDeleteOk();

    explicit QueueDeleteOk(bsl::uint32_t messageCount);

    size_t encodedSize() const { return sizeof(uint32_t); }

    bsl::uint32_t messageCount() const { return d_messageCount; }

    static bool decode(QueueDeleteOk* deleteOk,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const QueueDeleteOk& deleteOk);

  private:
    bsl::uint32_t d_messageCount;
};

bool operator==(const QueueDeleteOk& lhs, const QueueDeleteOk& rhs);

inline bool operator!=(const QueueDeleteOk& lhs, const QueueDeleteOk& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueDeleteOk& queueDeleteOk);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
