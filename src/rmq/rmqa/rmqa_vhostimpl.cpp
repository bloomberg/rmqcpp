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

#include <rmqa_vhostimpl.h>

#include <rmqt_exchange.h>
#include <rmqt_topology.h>

#include <bsl_cstdint.h>
#include <bsl_memory.h>
#include <bslmt_lockguard.h>

namespace BloombergLP {
namespace rmqa {
namespace {

rmqt::Future<rmqp::Producer>
proxyCreateProducerAsync(const bsl::shared_ptr<rmqp::Connection>& c,
                         const rmqt::Topology& topology,
                         rmqt::ExchangeHandle exchange,
                         uint16_t maxOutstandingConfirms)
{
    return c->createProducerAsync(topology, exchange, maxOutstandingConfirms);
}

rmqt::Future<rmqp::Consumer>
proxyCreateConsumerAsync(const bsl::shared_ptr<rmqp::Connection>& c,
                         const rmqt::Topology& topology,
                         rmqt::QueueHandle queue,
                         const rmqp::Consumer::ConsumerFunc& onMessage,
                         const rmqt::ConsumerConfig& config)
{
    return c->createConsumerAsync(topology, queue, onMessage, config);
}

void shutdownConnectionFuture(
    bslma::ManagedPtr<rmqt::Future<rmqp::Connection> >& connectionFuturePtr)
{
    if (connectionFuturePtr) {
        rmqt::Result<rmqp::Connection> connection =
            connectionFuturePtr->tryResult();
        if (connection) {
            connection.value()->close();
        }
        connectionFuturePtr.reset();
    }
}

} // namespace

VHostImpl::VHostImpl(const ConnectionMaker& connectionMaker)
: d_newConnection(connectionMaker)
, d_consumerFuture()
, d_producerFuture()
{
}

rmqt::Result<rmqp::Producer>
VHostImpl::createProducer(const rmqt::Topology& topology,
                          rmqt::ExchangeHandle exchange,
                          uint16_t maxOutstandingConfirms)
{
    return createProducerAsync(topology, exchange, maxOutstandingConfirms)
        .blockResult();
}

rmqt::Result<rmqp::Consumer>
VHostImpl::createConsumer(const rmqt::Topology& topology,
                          rmqt::QueueHandle queue,
                          const rmqp::Consumer::ConsumerFunc& onMessage,
                          const rmqt::ConsumerConfig& config)
{
    return createConsumerAsync(topology, queue, onMessage, config)
        .blockResult();
}

rmqt::Future<rmqp::Producer>
VHostImpl::createProducerAsync(const rmqt::Topology& topology,
                               rmqt::ExchangeHandle exchange,
                               uint16_t maxOutstandingConfirms)
{
    {
        bslmt::LockGuard<bslmt::Mutex> guard(&d_connectionMutex);
        if (!d_producerFuture) {
            d_producerFuture = bslma::ManagedPtrUtil::makeManaged<
                rmqt::Future<rmqp::Connection> >(d_newConnection("producer"));
        }
    }
    return d_producerFuture->thenFuture<rmqp::Producer>(
        rmqt::FutureUtil::propagateError<rmqp::Connection, rmqp::Producer>(
            bdlf::BindUtil::bind(&proxyCreateProducerAsync,
                                 bdlf::PlaceHolders::_1,
                                 topology,
                                 exchange,
                                 maxOutstandingConfirms))

    );
}

rmqt::Future<rmqp::Consumer>
VHostImpl::createConsumerAsync(const rmqt::Topology& topology,
                               rmqt::QueueHandle queue,
                               const rmqp::Consumer::ConsumerFunc& onMessage,
                               const rmqt::ConsumerConfig& config)
{
    {
        bslmt::LockGuard<bslmt::Mutex> guard(&d_connectionMutex);
        if (!d_consumerFuture) {
            d_consumerFuture = bslma::ManagedPtrUtil::makeManaged<
                rmqt::Future<rmqp::Connection> >(d_newConnection("consumer"));
        }
    }
    return d_consumerFuture->thenFuture<rmqp::Consumer>(
        rmqt::FutureUtil::propagateError<rmqp::Connection, rmqp::Consumer>(
            bdlf::BindUtil::bind(&proxyCreateConsumerAsync,
                                 bdlf::PlaceHolders::_1,
                                 topology,
                                 queue,
                                 onMessage,
                                 config)));
}

void VHostImpl::close()
{
    shutdownConnectionFuture(d_producerFuture);
    shutdownConnectionFuture(d_consumerFuture);
}
} // namespace rmqa
} // namespace BloombergLP
