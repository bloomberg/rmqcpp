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

#include <rmqt_queue.h>

#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqt {

Queue::Queue(const bsl::string& name /* = bsl::string() */,
             bool passive /* = false */,
             bool autoDelete /*  = false */,
             bool durable /* = true */,
             const rmqt::FieldTable& args /* = rmqt::FieldTable() */)
: d_name(name)
, d_passive(passive)
, d_autoDelete(autoDelete)
, d_durable(durable)
, d_args(args)
{
}

bsl::ostream& operator<<(bsl::ostream& os, const Queue& queue)
{
    os << "Queue = [ name = '" << queue.d_name << "' Properties = [";
    if (queue.d_passive) {
        os << "Passive";
    }
    if (queue.d_autoDelete) {
        os << "Auto-delete";
    }
    if (queue.d_durable) {
        os << "Durable";
    }
    os << "] Args = " << queue.d_args << " ]";

    return os;
}

} // namespace rmqt
} // namespace BloombergLP
