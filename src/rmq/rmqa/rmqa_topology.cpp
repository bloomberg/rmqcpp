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

#include <rmqa_topology.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bdlf_placeholder.h>

#include <bsl_algorithm.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqa {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.TOPOLOGY")

template <typename T>
bool topologyItemNameMatch(const bsl::shared_ptr<T>& a,
                           const bsl::shared_ptr<T>& b)
{
    return a == b || a->name() == b->name();
}

template <typename T>
bsl::shared_ptr<T> insertIfNotDupe(const bsl::shared_ptr<T>& newItem,
                                   bsl::vector<bsl::shared_ptr<T> >& container)
{
    using bdlf::PlaceHolders::_1;
    bsl::shared_ptr<T> result;
    if (bsl::find_if(
            container.cbegin(),
            container.cend(),
            bdlf::BindUtil::bind(&topologyItemNameMatch<T>, newItem, _1)) ==
        container.end()) {
        result = newItem;
        container.push_back(newItem);
    }
    return result;
}
} // namespace

Topology::Topology()
: d_topology()
, d_defaultExchange()
{
}
Topology::~Topology() {}

rmqt::QueueHandle Topology::addQueue(const bsl::string& name,
                                     rmqt::AutoDelete::Value autoDelete,
                                     rmqt::Durable::Value durable,
                                     const rmqt::FieldTable& args)
{
    return addQueueImpl(
        bsl::make_shared<rmqt::Queue>(name, false, autoDelete, durable, args));
}

rmqt::ExchangeHandle
Topology::addExchange(const bsl::string& name,
                      const rmqt::ExchangeType& exchangeType,
                      rmqt::AutoDelete::Value autoDelete,
                      rmqt::Durable::Value durable,
                      rmqt::Internal::Value internal,
                      const rmqt::FieldTable& args)
{
    return addExchangeImpl(bsl::make_shared<rmqt::Exchange>(
        name, false, exchangeType, autoDelete, durable, internal, args));
}

rmqt::ExchangeHandle
Topology::addExchangeImpl(const bsl::shared_ptr<rmqt::Exchange>& exchange)
{
    if (!rmqt::ExchangeUtil::validateName(exchange->name())) {
        BALL_LOG_ERROR << "Exchange name '" << exchange->name()
                       << "' is invalid.";
        return bsl::weak_ptr<rmqt::Exchange>();
    }
    bsl::shared_ptr<rmqt::Exchange> result =
        insertIfNotDupe(exchange, d_topology.exchanges);
    if (!result) {
        BALL_LOG_ERROR
            << "Exchange added to topology with duplicate name, name:"
            << exchange->name();
    }
    return result;
}

rmqt::QueueHandle
Topology::addQueueImpl(const bsl::shared_ptr<rmqt::Queue>& queue)
{
    bsl::shared_ptr<rmqt::Queue> result =
        insertIfNotDupe(queue, d_topology.queues);
    if (!result) {
        BALL_LOG_ERROR << "Queue added to topology with duplicate name, name:"
                       << queue->name();
    }
    return result;
}

rmqt::ExchangeHandle Topology::addPassiveExchange(const bsl::string& name)
{
    return addExchangeImpl(bsl::make_shared<rmqt::Exchange>(name, true));
}

rmqt::QueueHandle Topology::addPassiveQueue(const bsl::string& name)
{
    return addQueueImpl(bsl::make_shared<rmqt::Queue>(name, true));
}

void Topology::bind(const rmqt::ExchangeHandle& exchange,
                    const rmqt::QueueHandle& queue,
                    const bsl::string& bindingKey,
                    const rmqt::FieldTable& args)
{
    bsl::shared_ptr<rmqt::QueueBinding> bindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, bindingKey, args);
    d_topology.queueBindings.push_back(bindingPtr);
}

void Topology::bind(const rmqt::ExchangeHandle& sourceExchange,
                    const rmqt::ExchangeHandle& destinationExchange,
                    const bsl::string& bindingKey,
                    const rmqt::FieldTable& args)
{

    bsl::shared_ptr<rmqt::ExchangeBinding> bindingPtr =
        bsl::make_shared<rmqt::ExchangeBinding>(
            sourceExchange, destinationExchange, bindingKey, args);
    d_topology.exchangeBindings.push_back(bindingPtr);
}

const rmqt::Topology& Topology::topology() const { return d_topology; }

const rmqt::ExchangeHandle Topology::defaultExchange()
{
    if (d_defaultExchange.expired()) {
        d_defaultExchange = addExchangeImpl(bsl::make_shared<rmqt::Exchange>(
            bsl::string(rmqt::Exchange::DEFAULT_EXCHANGE),
            false,
            rmqt::ExchangeType::DIRECT,
            false,
            true,
            false,
            rmqt::FieldTable()));
    }
    return d_defaultExchange;
}

bsl::ostream& operator<<(bsl::ostream& os, const Topology& topology)
{
    os << topology.d_topology;
    return os;
}

} // namespace rmqa
} // namespace BloombergLP
