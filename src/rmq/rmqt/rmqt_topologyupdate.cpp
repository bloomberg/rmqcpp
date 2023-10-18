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

#include <rmqt_topologyupdate.h>

namespace BloombergLP {
namespace rmqt {

TopologyUpdate::TopologyUpdate()
: updates()
{
}

namespace {

class PrintingVisitor {
  public:
    explicit PrintingVisitor(bsl::ostream& os)
    : os(os)
    {
    }

    bsl::ostream& os;

    template <typename T>
    void operator()(const bsl::shared_ptr<T>& ptr)
    {
        if (!(ptr)) {
            os << "nullptr,";
        }
        else {
            os << *ptr << ",";
        }
    }

    template <typename T>
    void operator()(const T& /*ptr*/)
    {
        os << "NIL,";
    }
};

bsl::ostream& operator<<(bsl::ostream& os,
                         const TopologyUpdate::UpdatesVec& list)
{
    PrintingVisitor pv(os);

    os << "[";
    for (TopologyUpdate::UpdatesVec::const_iterator it = list.begin();
         it != list.end();
         ++it) {
        it->apply(pv);
    }
    os << "]";

    return os;
}
} // namespace

bsl::ostream& operator<<(bsl::ostream& os, const TopologyUpdate& topology)
{
    os << "TopologyUpdate = [ Updates = " << topology.updates << " ]";

    return os;
}

} // namespace rmqt
} // namespace BloombergLP
