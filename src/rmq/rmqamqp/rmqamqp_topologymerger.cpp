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

#include <rmqamqp_topologymerger.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bsl_algorithm.h>
#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.TOPOLOGYMERGER")

template <typename T>
bool findByBinding(const bsl::string& exchangeName,
                   const bsl::string& queueName,
                   const bsl::string& bindingKey,
                   const bsl::shared_ptr<T>& binding)
{
    bsl::shared_ptr<rmqt::Exchange> exchange = binding->exchange().lock();
    if (!exchange) {
        return false;
    }
    bsl::shared_ptr<rmqt::Queue> queue = binding->queue().lock();
    if (!queue) {
        return false;
    }

    if (exchangeName == exchange->name() && queueName == queue->name() &&
        bindingKey == binding->bindingKey()) {
        return true;
    }
    return false;
}

} // namespace

void TopologyMerger::merge(rmqt::Topology& topology,
                           const rmqt::TopologyUpdate& topologyUpdate)
{
    for (rmqt::TopologyUpdate::UpdatesVec::const_iterator it =
             topologyUpdate.updates.cbegin();
         it != topologyUpdate.updates.cend();
         ++it) {
        const rmqt::TopologyUpdate::SupportedUpdate& update = *it;
        if (update.is<bsl::shared_ptr<rmqt::QueueBinding> >()) {
            const bsl::shared_ptr<rmqt::QueueBinding>& queueBinding =
                update.the<bsl::shared_ptr<rmqt::QueueBinding> >();
            bsl::shared_ptr<rmqt::Exchange> exchange =
                queueBinding->exchange().lock();
            if (!exchange) {
                BALL_LOG_ERROR
                    << "Exchange object passed to bind update was destructed. "
                       "Cannot apply bind to reconnect topology.";
                continue;
            }
            bsl::shared_ptr<rmqt::Queue> queue = queueBinding->queue().lock();
            if (!queue) {
                BALL_LOG_ERROR
                    << "Queue object passed to bind update was destructed. "
                       "Cannot apply bind to reconnect topology.";
                continue;
            }
            using bdlf::PlaceHolders::_1;
            rmqt::Topology::QueueBindingVec::const_iterator iter = bsl::find_if(
                topology.queueBindings.cbegin(),
                topology.queueBindings.cend(),
                bdlf::BindUtil::bind(findByBinding<rmqt::QueueBinding>,
                                     exchange->name(),
                                     queue->name(),
                                     queueBinding->bindingKey(),
                                     _1));
            if (iter == topology.queueBindings.cend()) {
                topology.queueBindings.push_back(
                    update.the<bsl::shared_ptr<rmqt::QueueBinding> >());
            }
        }
        else if (update.is<bsl::shared_ptr<rmqt::QueueUnbinding> >()) {
            const bsl::shared_ptr<rmqt::QueueUnbinding>& queueUnbinding =
                update.the<bsl::shared_ptr<rmqt::QueueUnbinding> >();
            bsl::shared_ptr<rmqt::Exchange> exchange =
                queueUnbinding->exchange().lock();
            if (!exchange) {
                BALL_LOG_ERROR
                    << "Exchange object passed to unbind update was "
                       "destructed. Cannot apply unbind to reconnect topology.";
                continue;
            }
            bsl::shared_ptr<rmqt::Queue> queue = queueUnbinding->queue().lock();
            if (!queue) {
                BALL_LOG_ERROR
                    << "Queue object passed to unbind update was destructed. "
                       "Cannot apply unbind to reconnect topology.";
                continue;
            }
            using bdlf::PlaceHolders::_1;
            topology.queueBindings.erase(
                bsl::remove_if(
                    topology.queueBindings.begin(),
                    topology.queueBindings.end(),
                    bdlf::BindUtil::bind(findByBinding<rmqt::QueueBinding>,
                                         exchange->name(),
                                         queue->name(),
                                         queueUnbinding->bindingKey(),
                                         _1)),
                topology.queueBindings.end());
        }
    }
}

} // namespace rmqamqp
} // namespace BloombergLP
