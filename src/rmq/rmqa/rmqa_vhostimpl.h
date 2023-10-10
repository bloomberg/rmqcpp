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

// rmqa_vhostimpl.h
#ifndef INCLUDED_RMQA_VHOSTIMPL
#define INCLUDED_RMQA_VHOSTIMPL

#include <rmqp_connection.h>
#include <rmqp_consumer.h>
#include <rmqp_producer.h>
#include <rmqt_topology.h>

#include <bsl_functional.h>
#include <bslma_managedptr.h>
#include <bslmt_mutex.h>

namespace BloombergLP {
namespace rmqa {

/// \brief A RabbitMQ VHost Implementation of the rmqp::Connection interface

class VHostImpl : public rmqp::Connection {
  public:
    typedef bsl::function<rmqt::Future<rmqp::Connection>(
        const bsl::string& suffix)>
        ConnectionMaker;

    explicit VHostImpl(const ConnectionMaker& connectionMaker);

    rmqt::Result<rmqp::Producer>
    createProducer(const rmqt::Topology& topology,
                   rmqt::ExchangeHandle exchange,
                   uint16_t maxOutstandingConfirms) BSLS_KEYWORD_OVERRIDE;

    rmqt::Result<rmqp::Consumer>
    createConsumer(const rmqt::Topology& topology,
                   rmqt::QueueHandle queue,
                   const rmqp::Consumer::ConsumerFunc& onMessage,
                   const rmqt::ConsumerConfig& config) BSLS_KEYWORD_OVERRIDE;

    rmqt::Future<rmqp::Producer>
    createProducerAsync(const rmqt::Topology& topology,
                        rmqt::ExchangeHandle exchange,
                        uint16_t maxOutstandingConfirms) BSLS_KEYWORD_OVERRIDE;

    rmqt::Future<rmqp::Consumer> createConsumerAsync(
        const rmqt::Topology& topology,
        rmqt::QueueHandle queue,
        const rmqp::Consumer::ConsumerFunc& onMessage,
        const rmqt::ConsumerConfig& config) BSLS_KEYWORD_OVERRIDE;

    void close() BSLS_KEYWORD_OVERRIDE;

  private:
    ConnectionMaker d_newConnection;
    bslmt::Mutex d_connectionMutex;
    bslma::ManagedPtr<rmqt::Future<rmqp::Connection> > d_consumerFuture;
    bslma::ManagedPtr<rmqt::Future<rmqp::Connection> > d_producerFuture;

  private:
    VHostImpl(const VHostImpl&) BSLS_KEYWORD_DELETED;
    VHostImpl& operator=(const VHostImpl&) BSLS_KEYWORD_DELETED;
};
} // namespace rmqa
} // namespace BloombergLP

#endif
