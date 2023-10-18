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

// rmqp_connection.h
#ifndef INCLUDED_RMQP_CONNECTION
#define INCLUDED_RMQP_CONNECTION

#include <rmqp_consumer.h>
#include <rmqp_producer.h>
#include <rmqt_consumerconfig.h>

#include <rmqt_future.h>
#include <rmqt_result.h>

#include <bsl_string.h>
#include <bsls_timeinterval.h>

//@PURPOSE: Provide a RabbitMQ Connection API
//
//@CLASSES:
//  rmqp::Connection: a RabbitMQ Connection API object

namespace BloombergLP {
namespace rmqp {

/// \brief Provide a RabbitMQ Connection API
///
/// Represents a RabbitMQ connection.

class Connection {
  public:
    // TYPES
    typedef enum { SUCCESS = 0, FAIL } ReturnCode;

    Connection();
    virtual ~Connection();

    // MANIPULATORS
    /// Create a producer for the given exchange using the `topology` provided.
    /// This method starts the process of declaring the topology, and creating
    /// the producer. The returned Future provides access to the Producer when
    /// it is ready.
    /// \param topology The topology declared by this producer. This topology
    ///                 will be re-declared on reconnection. The passed
    ///                 `exchange` must exist in the given `topology`
    /// \param exchange The exchange to which all produced messages will be
    ///                 published
    ///
    /// \param maxOutstandingConfirms The maximum number of confirms the
    ///            `Producer` will allow before blocking, and waiting for a
    ///            confirm from the broker
    virtual rmqt::Result<Producer>
    createProducer(const rmqt::Topology& topology,
                   rmqt::ExchangeHandle exchange,
                   uint16_t maxOutstandingConfirms) = 0;

    /// \brief Create an asynchronous consumer using the provided Topology.
    /// \param topology The RabbitMQ topology which will be declared on the
    ///        broker with this consumer.
    /// \param queue The `queue` to consume from. This queue must be contained
    ///        within `topology`.
    /// \param onMessage The callback to be invoked on each message. This will
    ///        be invoked from the RabbitContext threadpool.
    /// \param consumerConfig dictates any optional tunables for the
    ///        consumer, e.g. consumerTag, prefetchCount, threadpool to process
    ///        the messages on
    ///
    /// \return A result which will contain either the connected consumer
    ///         object which has been registered on the Event Loop thread or an
    ///         error.
    virtual rmqt::Result<rmqp::Consumer>
    createConsumer(const rmqt::Topology& topology,
                   rmqt::QueueHandle queue,
                   const rmqp::Consumer::ConsumerFunc& onMessage,
                   const rmqt::ConsumerConfig& consumerConfig) = 0;

    /// \brief Flush all data pending and then close the connection.
    /// Close the connection and invalidates all
    /// consumers and producers created through this
    /// connection.
    virtual void close() = 0;

    virtual rmqt::Future<Producer>
    createProducerAsync(const rmqt::Topology& topology,
                        rmqt::ExchangeHandle exchange,
                        uint16_t maxOutstandingConfirms) = 0;

    virtual rmqt::Future<rmqp::Consumer>
    createConsumerAsync(const rmqt::Topology& topology,
                        rmqt::QueueHandle queue,
                        const rmqp::Consumer::ConsumerFunc& onMessage,
                        const rmqt::ConsumerConfig& consumerConfig) = 0;

    // DEPRECATED
    /// Create an asynchronous consumer using the topology provided.
    /// This method also creates the `topology` on the target broker
    /// synchronously using the `timeout` provided.
    /// If the timeout expires, the converted bool value of `Result`
    /// object returned by this function will be `false`.
    /// The `value` of the result `Result::value()` will be still
    /// usable in this case though.
    /// User-defined callbacks are run on the thread-pool, as
    /// messages are received from the broker.
    virtual rmqt::Result<rmqp::Consumer>
    createConsumer(const rmqt::Topology& topology,
                   rmqt::QueueHandle queue,
                   const rmqp::Consumer::ConsumerFunc& messageConsumer,
                   const bsl::string& consumerTag,
                   uint16_t prefetchCount);

    virtual rmqt::Future<rmqp::Consumer>
    createConsumerAsync(const rmqt::Topology& topology,
                        rmqt::QueueHandle queue,
                        const rmqp::Consumer::ConsumerFunc& messageConsumer,
                        const bsl::string& consumerTag,
                        uint16_t prefetchCount);

}; // class Connection

} // namespace rmqp
} // namespace BloombergLP

#endif // ! INCLUDED_RMQP_CONNECTION
