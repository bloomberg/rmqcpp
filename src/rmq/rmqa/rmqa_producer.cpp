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

#include <rmqa_producer.h>

namespace BloombergLP {
namespace rmqa {

Producer::Producer(bslma::ManagedPtr<rmqp::Producer>& impl)
: d_impl(impl)
{
}

Producer::~Producer() {}

rmqp::Producer::SendStatus
Producer::send(const rmqt::Message& message,
               const bsl::string& routingKey,
               const rmqp::Producer::ConfirmationCallback& confirmCallback,
               const bsls::TimeInterval& timeout /* = bsls::TimeInterval() */)
{
    return d_impl->send(message, routingKey, confirmCallback, timeout);
}

rmqp::Producer::SendStatus
Producer::send(const rmqt::Message& message,
               const bsl::string& routingKey,
               rmqt::Mandatory::Value mandatoryFlag,
               const rmqp::Producer::ConfirmationCallback& confirmCallback,
               const bsls::TimeInterval& timeout /* = bsls::TimeInterval() */)
{
    return d_impl->send(
        message, routingKey, mandatoryFlag, confirmCallback, timeout);
}

rmqp::Producer::SendStatus
Producer::trySend(const rmqt::Message& message,
                  const bsl::string& routingKey,
                  const rmqp::Producer::ConfirmationCallback& confirmCallback)
{
    return d_impl->trySend(message, routingKey, confirmCallback);
}

rmqt::Result<> Producer::waitForConfirms(const bsls::TimeInterval& timeout)
{
    return d_impl->waitForConfirms(timeout);
}

rmqt::Result<>
Producer::updateTopology(const rmqa::TopologyUpdate& topologyUpdate,
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

} // namespace rmqa
} // namespace BloombergLP
