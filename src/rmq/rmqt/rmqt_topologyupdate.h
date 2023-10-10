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

// rmqt_topologyupdate.h
#ifndef INCLUDED_RMQT_TOPOLOGYUPDATE
#define INCLUDED_RMQT_TOPOLOGYUPDATE

#include <rmqt_queue.h>
#include <rmqt_queuebinding.h>
#include <rmqt_queuedelete.h>
#include <rmqt_queueunbinding.h>

#include <bdlb_variant.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqt {

class TopologyUpdate {
  public:
    TopologyUpdate();
    typedef bdlb::Variant<bsl::shared_ptr<QueueBinding>,
                          bsl::shared_ptr<QueueUnbinding>,
                          bsl::shared_ptr<QueueDelete> >
        SupportedUpdate;
    typedef bsl::vector<SupportedUpdate> UpdatesVec;

    UpdatesVec updates;

    friend bsl::ostream& operator<<(bsl::ostream& os,
                                    const TopologyUpdate& topology);
}; // class TopologyUpdate

bsl::ostream& operator<<(bsl::ostream& os, const TopologyUpdate& topology);

} // namespace rmqt
} // namespace BloombergLP
#endif
