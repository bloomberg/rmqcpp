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

#ifndef INCLUDED_RMQAMQPT_QUEUEBINDOK
#define INCLUDED_RMQAMQPT_QUEUEBINDOK

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide queue BIND-OK method
///
/// This method confirms that the bind was successful.

class QueueBindOk {
  public:
    static const int METHOD_ID = rmqamqpt::Constants::QUEUE_BINDOK;

    QueueBindOk();

    size_t encodedSize() const { return 0; }

    static bool
    decode(QueueBindOk* bind, const uint8_t* data, bsl::size_t dataLength);
    static void encode(Writer& output, const QueueBindOk& bind);
};

bool operator==(const QueueBindOk& lhs, const QueueBindOk& rhs);

inline bool operator!=(const QueueBindOk& lhs, const QueueBindOk& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueBindOk& queueBindOk);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
