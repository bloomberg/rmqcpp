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

// rmqa_connectionimpl.h
#ifndef INCLUDED_RMQA_CONNECTIONIMPL
#define INCLUDED_RMQA_CONNECTIONIMPL

#include <rmqa_consumer.h>
#include <rmqa_consumerimpl.h>
#include <rmqa_producerimpl.h>

#include <rmqamqp_connection.h>

#include <rmqio_eventloop.h>
#include <rmqio_resolver.h>

#include <rmqp_connection.h>
#include <rmqp_consumer.h>
#include <rmqp_producer.h>

#include <rmqt_credentials.h>
#include <rmqt_endpoint.h>
#include <rmqt_future.h>
#include <rmqt_properties.h>
#include <rmqt_result.h>

#include <bdlmt_threadpool.h>
#include <bslma_managedptr.h>
#include <bsls_keyword.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_string.h>

//@PURPOSE: Provide a RabbitMQ Connection API
//
//@CLASSES:
//  rmqa::ConnectionImpl: Implementation of rmqp::Connection

namespace BloombergLP {
namespace rmqa {

class ConnectionImpl : public rmqp::Connection,
                       public bsl::enable_shared_from_this<ConnectionImpl> {
  public:
    static bsl::shared_ptr<rmqa::ConnectionImpl>
    make(const bsl::shared_ptr<rmqamqp::Connection>& amqpConn,
         rmqio::EventLoop& eventLoop,
         bdlmt::ThreadPool& threadpool,
         const rmqt::ErrorCallback& errorCb,
         const rmqt::SuccessCallback& successCb,
         const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
         const rmqt::Tunables& tunables,
         const bsl::shared_ptr<rmqa::ConsumerImpl::Factory>& consumerFactory,
         const bsl::shared_ptr<rmqa::ProducerImpl::Factory>& producerFactory);

    void close() BSLS_KEYWORD_OVERRIDE;

    rmqt::Result<rmqp::Producer>
    createProducer(const rmqt::Topology& topology,
                   rmqt::ExchangeHandle exchange,
                   uint16_t maxOutstandingConfirms) BSLS_KEYWORD_OVERRIDE;

    rmqt::Result<rmqp::Consumer> createConsumer(
        const rmqt::Topology& topology,
        rmqt::QueueHandle queue,
        const rmqp::Consumer::ConsumerFunc& onMessage,
        const rmqt::ConsumerConfig& consumerConfig) BSLS_KEYWORD_OVERRIDE;

    rmqt::Future<rmqp::Producer>
    createProducerAsync(const rmqt::Topology& topology,
                        rmqt::ExchangeHandle exchange,
                        uint16_t maxOutstandingConfirms) BSLS_KEYWORD_OVERRIDE;

    rmqt::Future<rmqp::Consumer> createConsumerAsync(
        const rmqt::Topology& topology,
        rmqt::QueueHandle queue,
        const rmqp::Consumer::ConsumerFunc& onMessage,
        const rmqt::ConsumerConfig& consumerConfig) BSLS_KEYWORD_OVERRIDE;

    ~ConnectionImpl() BSLS_KEYWORD_OVERRIDE;

  private:
    ConnectionImpl(
        const bsl::shared_ptr<rmqamqp::Connection>& amqpConn,
        rmqio::EventLoop& loop,
        bdlmt::ThreadPool& threadPool,
        const rmqt::ErrorCallback& errorCb,
        const rmqt::SuccessCallback& successCb,
        const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
        const rmqt::Tunables& tunables,
        const bsl::shared_ptr<rmqa::ConsumerImpl::Factory>& consumerFactory,
        const bsl::shared_ptr<rmqa::ProducerImpl::Factory>& producerFactory);

    void ready(bool success = true);
    void doClose();

    void closeImpl();

    rmqt::Tunables d_tunables;
    bsl::shared_ptr<rmqamqp::Connection> d_connection;
    bdlmt::ThreadPool& d_threadPool;
    rmqio::EventLoop& d_eventLoop;
    rmqt::ErrorCallback d_onError;
    rmqt::SuccessCallback d_onSuccess;
    bsl::shared_ptr<rmqt::Endpoint> d_endpoint;
    ///< Held for logging
    bsl::shared_ptr<rmqa::ConsumerImpl::Factory> d_consumerFactory;
    bsl::shared_ptr<rmqa::ProducerImpl::Factory> d_producerFactory;
};

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_CONNECTION
