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

#ifndef INCLUDED_RMQT_QUEUEDELETE
#define INCLUDED_RMQT_QUEUEDELETE

#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief An AMQP queue delete
///
/// This class represents a queue.delete operation.

class QueueDelete {
  public:
    QueueDelete(const bsl::string& queueName,
                bool ifUnused,
                bool ifEmpty,
                bool noWait)
    : d_name(queueName)
    , d_ifUnused(ifUnused)
    , d_ifEmpty(ifEmpty)
    , d_noWait(noWait)
    {
    }

    const bsl::string& name() const { return d_name; }
    bool ifUnused() const { return d_ifUnused; }
    bool ifEmpty() const { return d_ifEmpty; }
    bool noWait() const { return d_noWait; }

    friend bsl::ostream& operator<<(bsl::ostream& os,
                                    const QueueDelete& queueDelete);

  private:
    bsl::string d_name;
    bool d_ifUnused;
    bool d_ifEmpty;
    bool d_noWait;
};

bsl::ostream& operator<<(bsl::ostream& os, const QueueDelete& queueDelete);

} // namespace rmqt
} // namespace BloombergLP
#endif
