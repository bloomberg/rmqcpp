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

#ifndef INCLUDED_RMQAMQP_TOPOLOGYMERGER
#define INCLUDED_RMQAMQP_TOPOLOGYMERGER

#include <rmqt_topology.h>
#include <rmqt_topologyupdate.h>

namespace BloombergLP {
namespace rmqamqp {

//@PURPOSE: Merge rmqt::Topology with rmqt::TopologyUpdate
//
class TopologyMerger {
  public:
    static void merge(rmqt::Topology& topology,
                      const rmqt::TopologyUpdate& topologyUpdate);
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
