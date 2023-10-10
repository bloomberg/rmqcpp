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

// rmqt_topology.h
#ifndef INCLUDED_RMQT_TOPOLOGY
#define INCLUDED_RMQT_TOPOLOGY

#include <rmqt_exchange.h>
#include <rmqt_exchangebinding.h>
#include <rmqt_queue.h>
#include <rmqt_queuebinding.h>

#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqt {

class Topology {
  public:
    Topology();

    typedef bsl::vector<bsl::shared_ptr<Queue> > QueueVec;
    typedef bsl::vector<bsl::shared_ptr<Exchange> > ExchangeVec;
    typedef bsl::vector<bsl::shared_ptr<QueueBinding> > QueueBindingVec;
    typedef bsl::vector<bsl::shared_ptr<ExchangeBinding> > ExchangeBindingVec;

    QueueVec queues;
    ExchangeVec exchanges;
    QueueBindingVec queueBindings;
    ExchangeBindingVec exchangeBindings;

    friend bsl::ostream& operator<<(bsl::ostream& os, const Topology& topology);
}; // class Topology

bsl::ostream& operator<<(bsl::ostream& os, const Topology& topology);

} // namespace rmqt
} // namespace BloombergLP
#endif
