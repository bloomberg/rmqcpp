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

#include <rmqt_topology.h>

namespace BloombergLP {
namespace rmqt {

Topology::Topology()
: queues()
, exchanges()
, queueBindings()
, exchangeBindings()
{
}

namespace {
template <typename T>
bsl::ostream& operator<<(bsl::ostream& os,
                         const bsl::vector<bsl::shared_ptr<T> >& list)
{
    os << "[";
    for (typename bsl::vector<bsl::shared_ptr<T> >::const_iterator it =
             list.begin();
         it != list.end();
         ++it) {
        if (!(*it)) {
            os << "nullptr,";
        }
        else {
            os << **it << ",";
        }
    }
    os << "]";

    return os;
}
} // namespace

bsl::ostream& operator<<(bsl::ostream& os, const Topology& topology)
{
    os << "Topology = [ Queues = " << topology.queues
       << " Exchanges = " << topology.exchanges
       << " QueueBindings = " << topology.queueBindings
       << " ExchangeBindings = " << topology.exchangeBindings << " ]";

    return os;
}

} // namespace rmqt
} // namespace BloombergLP
