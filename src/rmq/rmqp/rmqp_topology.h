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

#ifndef INCLUDED_RMQP_TOPOLOGY
#define INCLUDED_RMQP_TOPOLOGY

#include <rmqt_exchange.h>
#include <rmqt_exchangetype.h>
#include <rmqt_fieldvalue.h>
#include <rmqt_properties.h>
#include <rmqt_queue.h>
#include <rmqt_topology.h>

#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqp {

/// \brief An interface providing a manipulatable RabbitMQ topology structure
///
/// Represents the RabbitMQ topology, allowing declaring of Exchanges, Queues,
/// and Bindings. This object is passed to rmqa::VHost when creating
/// Producers and Consumers, which ensure this topology is available on the
/// broker.

class Topology {
  public:
    // CREATORS
    virtual ~Topology();

    /// \brief Declare a queue
    /// \param name Queue Name. See https://www.rabbitmq.com/queues.html#names
    ///        for name rules.
    /// \param autoDelete **Not recommended** When `ON` the queue is declared
    ///        as autodelete. An autodelete queue is deleted when it's last
    ///        consumer is cancelled. This could cause **data loss** if all
    ///        consumers are disconnected in an outage.
    /// \param durable When `ON` the queue declaration will survive broker
    ///        restarts. Persistent messages will also survive broker restart
    ///        in the queue. Message persistence is controlled by the
    ///        publisher.
    /// \param args Other queue properties (such as TTL) are controlled by
    ///        key-value properties held in a rmqt::FieldTable.
    ///
    /// \return QueueHandle which can be later used in producers/consumers.
    ///         The result type has pointer semantics and does not pass the
    ///         ownership to the caller.
    virtual rmqt::QueueHandle
    addQueue(const bsl::string& name      = bsl::string(),
             rmqt::AutoDelete::Value      = rmqt::AutoDelete::OFF,
             rmqt::Durable::Value         = rmqt::Durable::ON,
             const rmqt::FieldTable& args = rmqt::FieldTable()) = 0;

    /// \brief Declare an exchange
    ///
    /// \param name Exchange name. Maximum allowed length is 127 characters.
    ///        See
    ///        https://www.rabbitmq.com/amqp-0-9-1-reference.html#exchange.declare.exchange
    ///        for name rules.
    /// \param exchangeType The exchange type drives the meaning of the
    ///        routingKey when publishing to an exchange.
    /// \param autoDelete When `ON` the exchange is deleted after all
    ///        bindings are removed.
    /// \param durable When `ON` the exchange declaration will survive broker
    ///        restarts
    /// \param internal When `YES` the exchange is declared as an internal.
    ///        Internal exchange cannot be used directly by publishers. It can
    ///        be used, when bound to other exchange.
    /// \param args Further Exchange properties are controlled by key-value
    ///        properties defined in an rmqt::FieldTable.
    ///
    /// \return ExchangeHandle which can later be used in producers/consumers.
    ///         The result type has pointer semantics and does not pass the
    ///         ownership to the caller.
    virtual rmqt::ExchangeHandle addExchange(
        const bsl::string& name,
        const rmqt::ExchangeType& exchangeType = rmqt::ExchangeType::DIRECT,
        rmqt::AutoDelete::Value autoDelete     = rmqt::AutoDelete::OFF,
        rmqt::Durable::Value durable           = rmqt::Durable::ON,
        rmqt::Internal::Value internal         = rmqt::Internal::NO,
        const rmqt::FieldTable& args           = rmqt::FieldTable()) = 0;

    /// \brief Declare a dependency on Exchange (without creating it)
    ///
    /// \param name Exchange name.
    ///
    /// \return ExchangeHandle which can later be used in producers/consumers.
    /// The result type has pointer semantics and does not pass the ownership
    /// to the caller.
    virtual rmqt::ExchangeHandle
    addPassiveExchange(const bsl::string& name) = 0;

    /// \brief Declare a dependency on Queue (without creating it)
    ///
    /// \param name Queue name.
    ///
    /// \return QueueHandle which can later be used in producers/consumers.
    /// The result type has pointer semantics and does not pass the ownership
    /// to the caller.
    virtual rmqt::QueueHandle addPassiveQueue(const bsl::string& name) = 0;

    /// Bind a queue and an exchange.
    virtual void bind(const rmqt::ExchangeHandle& exchangeName,
                      const rmqt::QueueHandle& queue,
                      const bsl::string& bindingKey,
                      const rmqt::FieldTable& args = rmqt::FieldTable()) = 0;

    /// Bind two exchanges.
    virtual void bind(const rmqt::ExchangeHandle& exchange1,
                      const rmqt::ExchangeHandle& exchange2,
                      const bsl::string& bindingKey,
                      const rmqt::FieldTable& args = rmqt::FieldTable()) = 0;

    /// \brief Get a readonly copy of stored topology
    ///
    /// This is used internally by `rmqamqp` to send the topology to the broker
    virtual const rmqt::Topology& topology() const = 0;

    /// \brief Retrieve a handle for the default exchange
    ///
    /// The default exchange is an exchange named '' (length 0). It is a DIRECT
    /// exchange with automatic bindings for all queues within the vhost
    virtual const rmqt::ExchangeHandle defaultExchange() = 0;

}; // class Topology

} // namespace rmqp
} // namespace BloombergLP

#endif // ! INCLUDED_RMQP_TOPOLOGY
