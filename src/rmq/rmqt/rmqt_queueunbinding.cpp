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

#include <rmqt_queueunbinding.h>

#include <rmqt_exchange.h>
#include <rmqt_queue.h>

#include <bsl_memory.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqt {

bsl::ostream& operator<<(bsl::ostream& os, const QueueUnbinding& binding)
{
    bsl::shared_ptr<rmqt::Queue> queue       = binding.d_queue.lock();
    bsl::shared_ptr<rmqt::Exchange> exchange = binding.d_exchange.lock();
    os << "Unbinding = [ queue = " << (queue ? queue->name() : "nullptr")
       << " exchange = " << (exchange ? exchange->name() : "nullptr")
       << " bindingKey = " << binding.bindingKey()
       << " args = " << binding.args() << " ]";

    return os;
}

} // namespace rmqt
} // namespace BloombergLP
