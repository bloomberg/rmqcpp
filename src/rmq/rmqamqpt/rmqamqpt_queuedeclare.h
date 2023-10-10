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

#ifndef INCLUDED_RMQAMQPT_QUEUEDECLARE
#define INCLUDED_RMQAMQPT_QUEUEDECLARE

#include <rmqamqpt_constants.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide queue DECLARE method
///
/// This method creates or checks a queue. When creating a new queue the client
/// can specify various properties that control the durability of the queue and
/// its contents, and the level of sharing for the queue.

class QueueDeclare {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::QUEUE_DECLARE;

    QueueDeclare();

    QueueDeclare(const bsl::string& name,
                 bool passive,
                 bool durable,
                 bool exclusive,
                 bool autoDelete,
                 bool noWait,
                 const rmqt::FieldTable& arguments);

    bsl::size_t encodedSize() const
    {
        return sizeof(uint16_t) + 2 * sizeof(uint8_t) + sizeof(uint32_t) +
               d_name.size() + FieldValueUtil::encodedTableSize(d_arguments);
    }

    const bsl::string& name() const { return d_name; }

    bool passive() const { return d_passive; }

    bool durable() const { return d_durable; }

    bool exclusive() const { return d_exclusive; }

    bool autoDelete() const { return d_autoDelete; }

    bool noWait() const { return d_noWait; }

    const rmqt::FieldTable& arguments() const { return d_arguments; }

    static bool
    decode(QueueDeclare* declare, const uint8_t* data, bsl::size_t dataLength);
    static void encode(Writer& output, const QueueDeclare& declare);

  private:
    bsl::string d_name;
    bool d_passive;
    bool d_durable;
    bool d_exclusive;
    bool d_autoDelete;
    bool d_noWait;
    rmqt::FieldTable d_arguments;
};

bool operator==(const QueueDeclare& lhs, const QueueDeclare& rhs);

inline bool operator!=(const QueueDeclare& lhs, const QueueDeclare& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueDeclare& queueDeclare);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
