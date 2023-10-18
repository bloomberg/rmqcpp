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

// rmqp_producer.h
#ifndef INCLUDED_RMQP_PRODUCER
#define INCLUDED_RMQP_PRODUCER

#include <rmqt_queue.h>

#include <rmqp_topologyupdate.h>
#include <rmqt_confirmresponse.h>
#include <rmqt_exchange.h>
#include <rmqt_message.h>
#include <rmqt_result.h>
#include <rmqt_topology.h>

#include <bsls_timeinterval.h>

#include <bsl_functional.h>
#include <bsl_string.h>
#include <rmqt_future.h>

namespace BloombergLP {
namespace rmqp {

/// \class Producer
/// \brief RabbitMQ Producer API for publishing to a specific
///        Exchange.
///
/// A RabbitMQ Message Producer API object. A Producer is bound to a specific
/// Exchange. These objects are constructed by the Connection
class Producer {
  public:
    // TYPES
    /// \brief Possible results of rmqp::Producer#send.
    enum SendStatus { SENDING, DUPLICATE, TIMEOUT, INFLIGHT_LIMIT };

    /// \brief Invoked on receipt of message confirmation.
    ///
    /// The user-provided ConfirmationCallback is invoked once RabbitMQ broker
    /// provides a guarantee (publisher confirm) that the message is enqueued.
    ///
    /// A ConfirmationCallback implementation should perform any commit action,
    /// such as confirming to the sender of the message that the action will be
    /// completed. For example, an application which consumes from one queue
    /// and produces to another should send the acknowledgement to the first
    /// queue once the ConfirmationCallback is invoked from the publish.
    typedef bsl::function<void(const rmqt::Message&,
                               const bsl::string& routingKey,
                               const rmqt::ConfirmResponse&)>
        ConfirmationCallback;

    // CREATORS
    virtual ~Producer();

    // MANIPULATORS

    /// \brief Send a message with the given `routingKey` to the exchange
    /// targeted by the producer.
    ///
    /// The behavior of this method depends on the the number of unconfirmed
    /// messages (sent but not yet confirmed by the broker). If this number is
    /// smaller than the limit configured when calling
    /// rmqa::VHost#createProducer, this method will return immediately.
    /// Otherwise it will block until the unconfirmed message count drops below
    /// the limit.
    ///
    /// \param message         The message to be sent.
    /// \param routingKey      The routing key (e.g. topic or queue name)
    ///                        passed to the exchange.
    /// \param confirmCallback Called when the broker explicitly
    ///                        confirms/rejects
    ///                        the message. Messages are automatically retried
    ///                        on reconnection, in which case this method may
    ///                        be called some time after invoking `send`.
    /// \param timeout         How long to wait for as a relative timeout. If
    ///                        timeout is 0, the method will wait to send
    ///                        message indefinitely
    ///
    /// \return SENDING   Returned when the library accepts the message for
    ///                   sending.
    ///                   If the connection is lost before receiving the
    ///                   publisher confirm from the broker, the library will
    ///                   retry sending the message.
    /// \return DUPLICATE Returned if a message with the same GUID has
    ///                   already been sent and is awaiting a confirm from the
    ///                   broker. This indicates an issue with the application.
    ///                   To send the same message multiple times, a new
    ///                   rmqt::Message object must be created every time.
    /// \return TIMEOUT   Returned if a message couldn't be enqueued within the
    ///                   timeout time.
    virtual SendStatus
    send(const rmqt::Message& message,
         const bsl::string& routingKey,
         const rmqp::Producer::ConfirmationCallback& confirmCallback,
         const bsls::TimeInterval& timeout) = 0;

    /// \brief Send a message with the given `routingKey` to the exchange
    /// targeted by the producer.
    ///
    /// The behavior of this method depends on the the number of unconfirmed
    /// messages (sent but not yet confirmed by the broker). If this number is
    /// smaller than the limit configured when calling
    /// rmqa::VHost#createProducer, this method will return immediately.
    /// Otherwise it will block until the unconfirmed message count drops below
    /// the limit.
    ///
    /// \param message         The message to be sent.
    /// \param routingKey      The routing key (e.g. topic or queue name)
    ///                        passed to the exchange.
    /// \param mandatory  Specify the mandatory flag:
    ///                   RETURN_UNROUTABLE (Recommended): Any messages not
    ///                   passed to a queue are returned to the sender.
    ///                   confirmCallback will be invoked with a RETURN status.
    ///                   DISCARD_UNROUTABLE (**Dangerous**): Any messages not
    ///                   passed to a queue are confirmed by the broker. This
    ///                   will cause silent message loss in the event bindings
    ///                   aren't setup as expected.
    /// \param confirmCallback Called when the broker explicitly
    ///                        confirms/rejects
    ///                        the message. Messages are automatically retried
    ///                        on reconnection, in which case this method may
    ///                        be called some time after invoking `send`.
    /// \param timeout         How long to wait for as a relative timeout. If
    ///                        timeout is 0, the method will wait to send
    ///                        message indefinitely
    ///
    /// \return SENDING   Returned when the library accepts the message for
    ///                   sending.
    ///                   If the connection is lost before receiving the
    ///                   publisher confirm from the broker, the library will
    ///                   retry sending the message.
    /// \return DUPLICATE Returned if a message with the same GUID has
    ///                   already been sent and is awaiting a confirm from the
    ///                   broker. This indicates an issue with the application.
    ///                   To send the same message multiple times, a new
    ///                   rmqt::Message object must be created every time.
    /// \return TIMEOUT   Returned if a message couldn't be enqueued within the
    ///                   timeout time.
    virtual SendStatus
    send(const rmqt::Message& message,
         const bsl::string& routingKey,
         rmqt::Mandatory::Value mandatoryFlag,
         const rmqp::Producer::ConfirmationCallback& confirmCallback,
         const bsls::TimeInterval& timeout) = 0;

    /// \brief Send a message with the given `routingKey` to the exchange
    /// targeted by the producer.
    ///
    /// The behavior of this method depends on the the number of unconfirmed
    /// messages (sent but not yet confirmed by the broker). If this number is
    /// smaller than the limit configured when calling
    /// rmqa::VHost#createProducer, this method behaves exactly as the method
    /// `send`. Otherwise, unlike `send`, this method returns immediately with
    /// a result indicating that the unconfirmed message limit has been
    /// reached.
    ///
    /// \param message         The message to be sent.
    /// \param routingKey      The routing key (e.g. topic or queue name)
    ///                        passed to the exchange.
    /// \param confirmCallback Called when the broker explicitly
    ///                        confirms/rejects
    ///                        the message. Messages are automatically retried
    ///                        on reconnection, in which case this method may
    ///                        be called some time after invoking `send`.
    ///
    /// \return SENDING        Returned when the library accepts the message
    ///                        for sending.
    ///                        If the connection is lost before receiving the
    ///                        publisher confirm from the broker, the library
    ///                        will retry sending the message.
    /// \return DUPLICATE      Returned if a message with the same GUID has
    ///                        already been sent and is awaiting a confirm from
    ///                        the broker. This indicates an issue with the
    ///                        application. To send the same message multiple
    ///                        times, a new rmqt::Message object must be
    ///                        created every time.
    /// \return INFLIGHT_LIMIT Returned if the unconfirmed message limit has
    ///                        been reached.
    virtual SendStatus
    trySend(const rmqt::Message& message,
            const bsl::string& routingKey,
            const rmqp::Producer::ConfirmationCallback& confirmCallback) = 0;

    /// \brief Wait for all outstanding publisher confirms to arrive.
    ///
    /// This method allows
    ///
    /// \param timeout   How long to wait for. If timeout is 0, the method will
    ///                wait for confirms indefinitely.
    ///
    /// \return true   if all outstanding confirms have arrived.
    /// \return false  if waiting timed out.
    virtual rmqt::Result<>
    waitForConfirms(const bsls::TimeInterval& timeout) = 0;

    /// \brief Updates topology
    ///
    /// \return A Future which, when resolved, will contain the result of the
    /// update.
    virtual rmqt::Future<>
    updateTopologyAsync(const rmqt::TopologyUpdate& topologyUpdate) = 0;

  protected:
    Producer();

  private:
    Producer(const Producer&) BSLS_KEYWORD_DELETED;
    Producer& operator=(const Producer&) BSLS_KEYWORD_DELETED;

}; // class Producer

} // namespace rmqp
} // namespace BloombergLP

#endif // ! INCLUDED_RMQP_PRODUCER
