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

#ifndef INCLUDED_RMQAMQPT_QUEUEUNBIND
#define INCLUDED_RMQAMQPT_QUEUEUNBIND

#include <rmqamqpt_constants.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief  Provide queue UNBIND method
///
/// This method unbinds a queue from an exchange.

class QueueUnbind {
  public:
    static const int METHOD_ID = rmqamqpt::Constants::QUEUE_UNBIND;

    QueueUnbind();

    QueueUnbind(const bsl::string& queue,
                const bsl::string& exchange,
                const bsl::string& routingKey,
                const rmqt::FieldTable& arguments);

    bsl::size_t encodedSize() const
    {
        return sizeof(uint16_t) + 3 * sizeof(uint8_t) + sizeof(uint32_t) +
               d_queue.size() + d_exchange.size() + d_routingKey.size() +
               FieldValueUtil::encodedTableSize(d_arguments);
    }

    const bsl::string& queue() const { return d_queue; }

    const bsl::string& exchange() const { return d_exchange; }

    const bsl::string& routingKey() const { return d_routingKey; }

    const rmqt::FieldTable& arguments() const { return d_arguments; }

    static bool
    decode(QueueUnbind* bind, const uint8_t* data, bsl::size_t dataLength);
    static void encode(Writer& output, const QueueUnbind& bind);

  private:
    bsl::string d_queue;
    bsl::string d_exchange;
    bsl::string d_routingKey;
    rmqt::FieldTable d_arguments;
};

bool operator==(const QueueUnbind& lhs, const QueueUnbind& rhs);

inline bool operator!=(const QueueUnbind& lhs, const QueueUnbind& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueUnbind& queueBind);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
