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

#include <rmqa_topologyupdate.h>

#include <bdlf_bind.h>
#include <bsl_memory.h>
#include <bsl_stdexcept.h>

namespace BloombergLP {
namespace rmqa {

TopologyUpdate::TopologyUpdate()
: d_topologyUpdate()
{
}

TopologyUpdate::~TopologyUpdate() {}

void TopologyUpdate::bind(const rmqt::ExchangeHandle& exchange,
                          const rmqt::QueueHandle& queue,
                          const bsl::string& bindingKey,
                          const rmqt::FieldTable& args)
{

    bsl::shared_ptr<rmqt::QueueBinding> queueBinding =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, bindingKey, args);
    d_topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBinding));
}

void TopologyUpdate::unbind(const rmqt::ExchangeHandle& exchange,
                            const rmqt::QueueHandle& queue,
                            const bsl::string& bindingKey,
                            const rmqt::FieldTable& args)
{

    bsl::shared_ptr<rmqt::QueueUnbinding> queueUnbinding =
        bsl::make_shared<rmqt::QueueUnbinding>(
            exchange, queue, bindingKey, args);
    d_topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueUnbinding));
}

void TopologyUpdate::deleteQueue(const bsl::string& queueName,
                                 rmqt::QueueUnused::Value ifUnused,
                                 rmqt::QueueEmpty::Value ifEmpty)
{
    bsl::shared_ptr<rmqt::QueueDelete> queueDelete =
        bsl::make_shared<rmqt::QueueDelete>(
            queueName, ifUnused, ifEmpty, false);
    d_topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueDelete));
}

const rmqt::TopologyUpdate& TopologyUpdate::topologyUpdate() const
{
    return d_topologyUpdate;
}

} // namespace rmqa
} // namespace BloombergLP
