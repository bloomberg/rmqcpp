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

#ifndef INCLUDED_RMQAMQPT_QUEUEDELETE
#define INCLUDED_RMQAMQPT_QUEUEDELETE

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide queue DELETE method
///
/// This method deletes a queue. When a queue is deleted any pending messages
/// are sent to a dead-letter queue if this is defined in the server
/// configuration, and all consumers on the queue are cancelled.

class QueueDelete {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::QUEUE_DELETE;

    QueueDelete();

    QueueDelete(const bsl::string& name,
                bool ifUnused,
                bool ifEmpty,
                bool noWait);

    size_t encodedSize() const
    {
        // 16 bits for reserved, 8 bits for bool args, 8 bits for name size
        return sizeof(uint16_t) + 2 * sizeof(uint8_t) + d_name.size();
    }

    const bsl::string& name() const { return d_name; }

    bool ifUnused() const { return d_ifUnused; }

    bool ifEmpty() const { return d_ifEmpty; }

    bool noWait() const { return d_noWait; }

    static bool decode(QueueDelete* queueDelete,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const QueueDelete& queueDelete);

  private:
    bsl::string d_name;
    bool d_ifUnused;
    bool d_ifEmpty;
    bool d_noWait;
};

bool operator==(const QueueDelete& lhs, const QueueDelete& rhs);

inline bool operator!=(const QueueDelete& lhs, const QueueDelete& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueDelete& queueDelete);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
