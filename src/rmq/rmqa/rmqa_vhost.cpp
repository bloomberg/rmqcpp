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

#include <rmqa_vhost.h>

#include <rmqa_consumer.h>
#include <rmqa_producer.h>

#include <rmqt_future.h>
#include <rmqt_properties.h>

#include <bdlb_guidutil.h>
#include <bslma_managedptr.h>

#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqa {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.VHOST")
} // namespace

VHost::~VHost()
{
    BALL_LOG_TRACE << "~VHost";
    close();
}

VHost::VHost(bslma::ManagedPtr<rmqp::Connection> impl)
: d_impl(impl)
{
}

void VHost::close() { d_impl->close(); }

bsl::string VHost::generateConsumerTag()
{
    return rmqt::ConsumerConfig::generateConsumerTag();
}

rmqt::Result<Producer> VHost::createProducer(const rmqp::Topology& topology,
                                             rmqt::ExchangeHandle exchange,
                                             uint16_t maxOutstandingConfirms)
{
    return rmqt::FutureUtil::convertViaManagedPtr<rmqp::Producer,
                                                  rmqa::Producer>(
        d_impl->createProducer(
            topology.topology(), exchange, maxOutstandingConfirms));
}

rmqt::Result<Consumer>
VHost::createConsumer(const rmqp::Topology& topology,
                      rmqt::QueueHandle queue,
                      const rmqp::Consumer::ConsumerFunc& onMessage,
                      const rmqt::ConsumerConfig& config)
{
    return rmqt::FutureUtil::convertViaManagedPtr<rmqp::Consumer,
                                                  rmqa::Consumer>(
        d_impl->createConsumer(topology.topology(), queue, onMessage, config));
}

rmqt::Result<rmqa::Consumer> VHost::createConsumer(
    const rmqp::Topology& topology,
    rmqt::QueueHandle queue,
    const rmqp::Consumer::ConsumerFunc& onMessage,
    const bsl::string&
        consumerTag, /* =  rmqt::ConsumerConfig::generateConsumerTag() */
    uint16_t prefetchCount /* = rmqt::ConsumerConfig::s_defaultPrefetchCount */)
{

    return rmqt::FutureUtil::convertViaManagedPtr<rmqp::Consumer,
                                                  rmqa::Consumer>(
        d_impl->createConsumer(
            topology.topology(), queue, onMessage, consumerTag, prefetchCount));
}

rmqt::Result<> VHost::deleteQueue(const bsl::string& name,
                                  rmqt::QueueUnused::Value ifUnused,
                                  rmqt::QueueEmpty::Value ifEmpty,
                                  const bsls::TimeInterval& timeout)
{
    rmqa::Topology topology;
    rmqt::Future<Producer> producerFuture =
        createProducerAsync(topology, topology.defaultExchange(), 1);
    rmqt::Result<rmqa::Producer> producerResult =
        (timeout.totalNanoseconds() == 0) ? producerFuture.blockResult()
                                          : producerFuture.waitResult(timeout);
    if (!producerResult) {
        return rmqt::Result<>("Couldn't create a producer to delete queue: " +
                                  producerResult.error(),
                              producerResult.returnCode());
    }
    bsl::shared_ptr<rmqa::Producer> producer = producerResult.value();
    rmqa::TopologyUpdate topologyUpdate;
    topologyUpdate.deleteQueue(name, ifUnused, ifEmpty);
    return producer->updateTopology(topologyUpdate, timeout);
}

rmqt::Future<Producer>
VHost::createProducerAsync(const rmqp::Topology& topology,
                           rmqt::ExchangeHandle exchange,
                           uint16_t maxOutstandingConfirms)
{
    return d_impl
        ->createProducerAsync(
            topology.topology(), exchange, maxOutstandingConfirms)
        .then<rmqa::Producer>(
            &rmqt::FutureUtil::convertViaManagedPtr<rmqp::Producer,
                                                    rmqa::Producer>);
}

rmqt::Future<Consumer>
VHost::createConsumerAsync(const rmqp::Topology& topology,
                           rmqt::QueueHandle queue,
                           const rmqp::Consumer::ConsumerFunc& onMessage,
                           const rmqt::ConsumerConfig& config)
{
    return d_impl
        ->createConsumerAsync(topology.topology(), queue, onMessage, config)
        .then<rmqa::Consumer>(
            &rmqt::FutureUtil::convertViaManagedPtr<rmqp::Consumer,
                                                    rmqa::Consumer>);
}

rmqt::Future<Consumer> VHost::createConsumerAsync(
    const rmqp::Topology& topology,
    rmqt::QueueHandle queue,
    const rmqp::Consumer::ConsumerFunc& onMessage,
    const bsl::string&
        consumerTag, /* = rmqt::ConsumerConfig::generateConsumerTag() */
    uint16_t prefetchCount /*= rmqt::ConsumerConfig::s_defaultPrefetchCount*/)
{
    return d_impl
        ->createConsumerAsync(
            topology.topology(), queue, onMessage, consumerTag, prefetchCount)
        .then<rmqa::Consumer>(
            &rmqt::FutureUtil::convertViaManagedPtr<rmqp::Consumer,
                                                    rmqa::Consumer>);
}

} // namespace rmqa
} // namespace BloombergLP
