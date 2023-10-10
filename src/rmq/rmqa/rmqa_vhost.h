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

// rmqa_vhost.h
#ifndef INCLUDED_RMQA_VHOST
#define INCLUDED_RMQA_VHOST

#include <rmqa_topology.h>
#include <rmqp_connection.h>
#include <rmqp_consumer.h>
#include <rmqp_topology.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_credentials.h>
#include <rmqt_endpoint.h>
#include <rmqt_future.h>
#include <rmqt_properties.h>
#include <rmqt_queue.h>
#include <rmqt_result.h>

#include <bdlmt_threadpool.h>

#include <bslma_managedptr.h>
#include <bsls_timeinterval.h>

#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqa {

class Producer;
class Consumer;

/// \brief A RabbitMQ VHost object
///
/// Represents connections needed to communicate with a vhost on the RabbitMQ
/// broker. Useful to create Producer and Consumer channels/objects. The VHost
/// must live longer than Consumers and Producers created from it.

class VHost {
  public:
    // CREATORS
    /// VHost (connections) are constructed by
    /// RabbitContext#createVhostConnection
    explicit VHost(bslma::ManagedPtr<rmqp::Connection> impl);

    /// The RabbitMQ connection will be disconnected on this call.
    virtual ~VHost();

    // MANIPULATORS
    /// \brief Create a producer using the provided Topology.
    /// This method starts the process of declaring the topology, and creating
    /// the producer. The returned Future provides access to the Producer when
    /// it is ready.
    /// \param topology The RabbitMQ topology declared by this producer. This
    ///        topology will be automatically re-declared on reconnection.
    /// \param exchange The exchange all messages will be published to. This
    ///        exchange must be contained in `topology` for the Producer to be
    ///        successfully created.
    /// \param maxOutstandingConfirms The maximum number of unconfirmed
    ///        messages `Producer` will allow before blocking and waiting for a
    ///        publisher confirm from the broker.
    ///
    /// \return A result which will either be a connected producer that has
    /// been registered on the Event Loop thread or an error.
    ///
    /// \note The VHost object must outlive the Producer
    rmqt::Result<Producer> createProducer(const rmqp::Topology& topology,
                                          rmqt::ExchangeHandle exchange,
                                          uint16_t maxOutstandingConfirms);

    /// \brief Create an asynchronous consumer using the provided Topology.
    /// \param topology The RabbitMQ topology which will be declared on the
    ///        broker with this consumer.
    /// \param queue The `queue` to consume from. This queue must be contained
    ///        within `topology`.
    /// \param onMessage The callback to be invoked on each message. This will
    ///        be invoked from the RabbitContext threadpool.
    /// \param config additional options like the prefetchCount and the
    ///               consumerTag
    ///
    /// \return A result which will contain either the connected consumer
    ///         object which has been registered on the Event Loop thread or an
    ///         error.
    ///
    /// \note The VHost object must outlive the Consumer
    rmqt::Result<Consumer>
    createConsumer(const rmqp::Topology& topology,
                   rmqt::QueueHandle queue,
                   const rmqp::Consumer::ConsumerFunc& onMessage,
                   const rmqt::ConsumerConfig& config = rmqt::ConsumerConfig());

    /// \deprecated
    /// \brief Create an asynchronous consumer using the provided Topology.
    /// \param topology The RabbitMQ topology which will be declared on the
    ///        broker with this consumer.
    /// \param queue The `queue` to consume from. This queue must be contained
    ///        within `topology`.
    /// \param consumerTag A label for the consumer which is displayed on the
    ///        RabbitMQ Management UI. It is useful to give this a meaningful
    ///        name.
    /// \param onMessage The callback to be invoked on each message. This will
    ///        be invoked from the RabbitContext threadpool.
    /// \param prefetchCount Used by the RabbitMQ broker to limit the number of
    ///        messages held by a consumer at one time. Higher values can
    ///        increase throughput, particularly in high latency environments.
    ///
    /// \return A result which will contain either the connected consumer
    ///         object which has been registered on the Event Loop thread or an
    ///         error.
    ///
    /// \note The VHost object must outlive the Consumer
    rmqt::Result<Consumer> createConsumer(
        const rmqp::Topology& topology,
        rmqt::QueueHandle queue,
        const rmqp::Consumer::ConsumerFunc& onMessage,
        const bsl::string& consumerTag,
        uint16_t prefetchCount = rmqt::ConsumerConfig::s_defaultPrefetchCount);

    /// \brief Close the connection to the broker.
    ///        The connection will not reconnect after this call.
    void close();

    /// \deprecated use rmqt::ConsumerConfig::generateConsumerTag()
    static bsl::string generateConsumerTag();

#ifdef USES_LIBRMQ_EXPERIMENTAL_FEATURES
    /// \brief Delete a queue from the vhost.
    /// This method deletes a queue from the vhost. On confirmation from the
    /// broker the number of messages deleted along with the queue is logged at
    /// INFO level, but not the name of the queue.
    /// If the queue does not exist on the vhost, broker will confirm this
    /// operation as successful.
    /// \param name Name of the queue to delete.
    /// \param ifUnused If set to IF_UNUSED,
    ///        broker will delete the queue only
    ///        if the queue has no active consumers. If set to ALLOW_IN_USE,
    ///        broker will delete the queue regardless of any consumers.
    /// \param ifEmpty If set to IF_EMPTY, broker will delete the queue only if
    ///        there are no messages in it. If set to ALLOW_MSG_DELETE, broker
    ///        will delete the queue along with any messages in it.
    /// \param timeout Timeout for the operation. If timeout is 0, the call is
    ///        blocking. IMPORTANT: due to implementation details the method
    ///        may take up to 2x timeout to complete.
    ///
    /// \return A result which evaluates to `True` if the queue was
    ///         successfully deleted, error result otherwise.
    rmqt::Result<>
    deleteQueue(const bsl::string& name,
                rmqt::QueueUnused::Value ifUnused,
                rmqt::QueueEmpty::Value ifEmpty,
                const bsls::TimeInterval& timeout = bsls::TimeInterval(0));

    /// NOTE: These async API is likely to move/change in the near future. Do
    /// not use without permission
    rmqt::Future<Producer> createProducerAsync(const rmqp::Topology& topology,
                                               rmqt::ExchangeHandle exchange,
                                               uint16_t maxOutstandingConfirms);

    rmqt::Future<Consumer> createConsumerAsync(
        const rmqp::Topology& topology,
        rmqt::QueueHandle queue,
        const rmqp::Consumer::ConsumerFunc& onMessage,
        const rmqt::ConsumerConfig& config = rmqt::ConsumerConfig());

    // DEPRECATED
    rmqt::Future<Consumer> createConsumerAsync(
        const rmqp::Topology& topology,
        rmqt::QueueHandle queue,
        const rmqp::Consumer::ConsumerFunc& onMessage,
        const bsl::string& consumerTag,
        uint16_t prefetchCount = rmqt::ConsumerConfig::s_defaultPrefetchCount);
#endif

  private:
    VHost(const VHost&) BSLS_KEYWORD_DELETED;
    VHost& operator=(const VHost&) BSLS_KEYWORD_DELETED;

  private:
    bslma::ManagedPtr<rmqp::Connection> d_impl;
};

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_VHOST
