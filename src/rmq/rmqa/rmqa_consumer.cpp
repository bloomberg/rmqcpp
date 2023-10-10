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

#include <rmqa_consumer.h>

#include <rmqp_consumer.h>

#include <rmqt_result.h>

#include <bdlf_bind.h>
#include <bdlf_placeholder.h>
#include <bslma_managedptr.h>

namespace BloombergLP {
namespace rmqa {

Consumer::Consumer(bslma::ManagedPtr<rmqp::Consumer>& impl)
: d_impl(impl)
{
}

void Consumer::cancel()
{
    rmqt::Result<> cancelResult =
        d_impl->cancel().waitResult(bsls::TimeInterval(0, 1000));
    BALL_LOG_SET_CATEGORY("Consumer::cancel");
    BALL_LOG_INFO << "Cancel Called, result after 1ms: " << cancelResult;
}

rmqt::Result<> Consumer::cancelAndDrain(
    const bsls::TimeInterval& timeout /* = bsls::TimeInterval(0)*/)
{
    return d_impl->cancelAndDrain(timeout);
}

rmqt::Result<>
Consumer::updateTopology(const rmqa::TopologyUpdate& topologyUpdate,
                         const bsls::TimeInterval& timeout)
{
    rmqt::Future<> future =
        d_impl->updateTopologyAsync(topologyUpdate.topologyUpdate());
    rmqt::Result<> result;
    if (timeout.totalNanoseconds() == 0) {
        result = future.blockResult();
    }
    else {
        result = future.waitResult(timeout);
    }
    return result;
}

Consumer::~Consumer() {}
} // namespace rmqa
} // namespace BloombergLP
