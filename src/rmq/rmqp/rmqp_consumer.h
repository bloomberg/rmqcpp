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

#ifndef INCLUDED_RMQP_CONSUMER
#define INCLUDED_RMQP_CONSUMER

#include <rmqp_messageguard.h>
#include <rmqp_topologyupdate.h>
#include <rmqt_future.h>
#include <rmqt_message.h>
#include <rmqt_result.h>
#include <rmqt_topology.h>
#include <rmqt_topologyupdate.h>

#include <bsl_functional.h>

namespace BloombergLP {
namespace rmqp {

/// \brief Provide a RabbitMQ Async Consumer API
///
/// Consumer represents a RabbitMQ consumer in a dedicated channel on the
/// Connection. The Consumer::Consumer implementation passed to
/// Connection#createConsumer is invoked on each message sent by the broker

class Consumer {
  public:
    // TYPES

    /// \brief Callback function used to receive messages.
    ///
    /// The passed implementation is invoked on each message received from the
    /// broker. rmqp::MessageGuard is used to pass positive or negative
    /// acknowledgments to the broker after processing. The callback will
    /// always be invoked from the RabbitContext threadpool.
    typedef bsl::function<void(rmqp::MessageGuard&)> ConsumerFunc;

    // CREATORS
    /// Consumer is constructed from the Connection object.
    Consumer();

    /// \brief Cancels the consumer, stops new messages flowing in.
    /// \return A Future which, when resolved, confirms that the server won't
    /// send any more messages.
    virtual rmqt::Future<> cancel() = 0;

    /// \brief Can only be called (successfully) once the Consumer has been
    /// cancelled, returns a future which resolves when all outstanding acks
    /// have been sent to the server, \note: this relies on the consumer code
    /// processing all of the outstanding messages received.
    virtual rmqt::Future<> drain() = 0;

    /// \brief Tells the broker to stop delivering messages to this consumer.
    /// \param timeout   How long to wait for all delivered (unacked) messages
    /// to be processed by the user provided callback. If timeout is 0, the
    /// method will wait indefinitely for them to complete, unless the client
    /// is disconnected from the broker in between.
    /// \return a result once all of the remaining messages
    /// have been n/acked by the consuming code, or error otherwise e.g.
    /// timeout
    /// \deprecated
    ///
    /// \note this relies on the consumer code processing
    /// all of the outstanding messages.
    virtual rmqt::Result<>
    cancelAndDrain(const bsls::TimeInterval& timeout) = 0;

    /// \brief Updates topology
    ///
    /// \return A Future which, when resolved, will contain the result of
    /// the update.
    virtual rmqt::Future<>
    updateTopologyAsync(const rmqt::TopologyUpdate& topologyUpdate) = 0;

    virtual ~Consumer();

  private:
    Consumer(const Consumer&) BSLS_KEYWORD_DELETED;
    Consumer& operator=(const Consumer&) BSLS_KEYWORD_DELETED;

}; // class Consumer

} // namespace rmqp
} // namespace BloombergLP

#endif // ! INCLUDED_RMQP_CONSUMER
