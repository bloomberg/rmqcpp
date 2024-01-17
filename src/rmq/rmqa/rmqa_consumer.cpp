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
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.CONSUMER")

} // namespace

Consumer::Consumer(bslma::ManagedPtr<rmqp::Consumer>& impl)
: d_impl(impl)
{
}

rmqt::Result<> Consumer::cancel(const bsls::TimeInterval& timeout)
{
    rmqt::Future<> cancelFuture = d_impl->cancel();
    rmqt::Result<> cancelResult = timeout == bsls::TimeInterval(0)
                                      ? cancelFuture.blockResult()
                                      : cancelFuture.waitResult(timeout);

    if (timeout > bsls::TimeInterval(0)) {
        BALL_LOG_INFO << "Cancel Called, result after " << timeout << ": "
                      << cancelResult;
    }
    return cancelResult;
}

rmqt::Result<> Consumer::resume(const bsls::TimeInterval& timeout)
{
    rmqt::Future<> resumeFuture = d_impl->resume();
    rmqt::Result<> resumeResult = timeout == bsls::TimeInterval(0)
                                      ? resumeFuture.blockResult()
                                      : resumeFuture.waitResult(timeout);

    if (timeout > bsls::TimeInterval(0)) {
        BALL_LOG_INFO << "Resume Called, result after " << timeout << ": "
                      << resumeResult;
    }
    return resumeResult;
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
