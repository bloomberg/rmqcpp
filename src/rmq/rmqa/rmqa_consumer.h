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

// rmqa_consumer.h
#ifndef INCLUDED_RMQA_CONSUMER
#define INCLUDED_RMQA_CONSUMER

#include <rmqa_messageguard.h>
#include <rmqa_topologyupdate.h>

#include <rmqp_consumer.h>
#include <rmqp_topologyupdate.h>
#include <rmqt_future.h>
#include <rmqt_message.h>
#include <rmqt_result.h>

#include <bsl_functional.h>
#include <bslma_managedptr.h>

namespace BloombergLP {
namespace rmqa {

/// \brief Provide a RabbitMQ Async Consumer API
///
/// Consumer represents a RabbitMQ consumer in a dedicated channel on the
/// Connection. The Consumer#Consumer implementation passed to
/// Connection#createConsumer is invoked on each message sent by the broker

class Consumer {
  public:
    // INNER CLASSES
    /// Create an instance of a `Consumer` that will operate
    /// through the supplied async consumer implementation.
    /// The `Consumer` object takes ownership of the supplied
    /// asynchronous consumer implementation.
    /// Consumer should generally be constructed via
    /// Connection#createConsumer.
    explicit Consumer(bslma::ManagedPtr<rmqp::Consumer>& impl);

    /// \brief Tells the broker to stop delivering messages to this consumer.
    /// it's still possible to nack/ack messages from callbacks after cancel is
    /// called
    void cancel();

    /// \brief Tells the broker to stop delivering messages to this consumer.
    /// \param timeout   How long to wait for all delivered (unacked) messages
    /// to be processed by the user provided callback. If timeout is 0, the
    /// method will wait indefinitely for them to complete, unless the client
    /// is disconnected from the broker in between.
    /// \return a result once all
    /// of the remaining messages have been n/acked by the consuming code, or
    /// error otherwise e.g. timeout
    /// \warning This should never be called from a message processing callback
    /// \note this relies on the consumer code processing
    /// all of the outstanding messages
    rmqt::Result<>
    cancelAndDrain(const bsls::TimeInterval& timeout = bsls::TimeInterval(0));

    /// Updates topology and waits for the server to confirm the update status
    ///
    /// \param timeout   How long to wait for. If timeout is 0, the method will
    ///                indefinitely wait for confirms.
    ///
    /// \return truthy   if all outstanding confirms have arrived.
    /// \return falsey  if update failed or waiting timed out, with an
    /// associated error message.
    rmqt::Result<>
    updateTopology(const rmqa::TopologyUpdate& topologyUpdate,
                   const bsls::TimeInterval& timeout = bsls::TimeInterval(0));

    class Factory;
    // Internal implementation used by Connection.

    // CREATORS
    /// Destructor stops the consumer.
    ~Consumer();

  private:
    Consumer(const Consumer&) BSLS_KEYWORD_DELETED;
    Consumer& operator=(const Consumer&) BSLS_KEYWORD_DELETED;

  private:
    bslma::ManagedPtr<rmqp::Consumer> d_impl;
}; // class Consumer

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_CONSUMER
