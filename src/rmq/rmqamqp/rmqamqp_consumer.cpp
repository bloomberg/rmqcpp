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

// rmqamqp_consumer.cpp   -*-C++-*-
#include <rmqamqp_consumer.h>

#include <rmqamqpt_basicconsume.h>
#include <rmqamqpt_basicdeliver.h>
#include <rmqamqpt_basicqos.h>
#include <rmqt_consumerack.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_envelope.h>
#include <rmqt_fieldvalue.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bdlt_currenttime.h>
#include <bsls_assert.h>

#include <bsl_algorithm.h>
#include <bsl_map.h>
#include <bsl_optional.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.CONSUMER")

rmqt::FieldTable
getBasicConsumeArguments(const rmqt::ConsumerConfig& consumerConfig)
{
    rmqt::FieldTable arguments;

    bsl::optional<int64_t> consumerPriority = consumerConfig.consumerPriority();
    if (consumerPriority) {
        arguments["x-priority"] = rmqt::FieldValue(consumerPriority.value());
    }

    return arguments;
}

} // namespace

Consumer::Consumer(const bsl::shared_ptr<rmqt::Queue>& queue,
                   const MessageCallback& onNewMessage,
                   const bsl::string& consumerTag)
: d_state(NOT_CONSUMING)
, d_tag(consumerTag)
, d_queue(queue)
, d_onNewMessage(onNewMessage)
{
}

void Consumer::reset()
{
    if (!isCancelling()) {
        d_state = NOT_CONSUMING;
    }
}

void Consumer::resume()
{
    switch (d_state) {
        case CANCELLED:
            // If resume is called after receiving cancelOK, on a CANCELLED
            // state, the state is changed to NOT_CONSUMING so that we can
            // consume later
            d_state = NOT_CONSUMING;
            break;
        case CANCELLING:
            // If resume is called before receiving cancelOK, on a CANCELLING
            // state, the state is changed to CANCELLING_BUT_RESUMING so that we
            // can consume later
            d_state = CANCELLING_BUT_RESUMING;
            break;
        case STARTING:
        case CONSUMING:
        case NOT_CONSUMING:
        case CANCELLING_BUT_RESUMING:
            BALL_LOG_INFO << "Resume called in a state: " << d_state
                          << ", ignoring";
            break;
    }
}

bsl::optional<rmqamqpt::BasicMethod> Consumer::consumeOk()
{
    bsl::optional<rmqamqpt::BasicMethod> method;

    switch (d_state) {
        case STARTING:
            d_state = CONSUMING;
            break;
        case CANCELLING:
            // Consumer has been cancelled since sending Basic.Consume
            BALL_LOG_INFO
                << "Received ConsumeOk for a cancelled consumer, cancelling";
            method = rmqamqpt::BasicCancel(d_tag);
            break;
        default:
            BALL_LOG_ERROR << "Unexpected ConsumeOK, consumer-tag:  " << d_tag
                           << ", state: " << d_state << ", ignoring";
    }
    return method;
}

const bsl::string& Consumer::consumerTag() const { return d_tag; }

const bsl::string& Consumer::queueName() const { return d_queue->name(); }

bool Consumer::isStarted() const
{
    return d_state != NOT_CONSUMING || d_state != CANCELLED;
}
bool Consumer::isActive() const { return d_state == CONSUMING; }
bool Consumer::isCancelling() const { return d_state > CONSUMING; }

void Consumer::process(const rmqt::Message& msg,
                       const rmqamqpt::BasicDeliver& deliver,
                       size_t lifetimeId)
{
    d_onNewMessage(msg,
                   rmqt::Envelope(deliver.deliveryTag(),
                                  lifetimeId,
                                  deliver.consumerTag(),
                                  deliver.exchange(),
                                  deliver.routingKey(),
                                  deliver.redelivered()));
}

bsl::optional<rmqamqpt::BasicMethod>
Consumer::consume(const rmqt::ConsumerConfig& consumerConfig)
{
    bsl::optional<rmqamqpt::BasicMethod> method;

    if (d_state == NOT_CONSUMING || d_state == CANCELLED ||
        d_state == CANCELLING_BUT_RESUMING) {
        BALL_LOG_INFO << "Starting consumer: " << d_tag
                      << " for queue: " << d_queue->name();
        d_state = STARTING;

        const bool noLocal = false;
        const bool noAck   = false;
        const bool exclusive =
            consumerConfig.exclusiveFlag() == rmqt::Exclusive::ON;
        const bool noWait = false;

        method =
            rmqamqpt::BasicConsume(d_queue->name(),
                                   d_tag,
                                   getBasicConsumeArguments(consumerConfig),
                                   noLocal,
                                   noAck,
                                   exclusive,
                                   noWait);
    }
    else {
        BALL_LOG_ERROR << "Start called in state " << d_state << ", ignoring";
    }

    return method;
}

bsl::optional<rmqamqpt::BasicMethod> Consumer::cancel()
{
    bsl::optional<rmqamqpt::BasicMethod> method;

    if (d_state == NOT_CONSUMING) {
        d_state = CANCELLED;
        return method;
    }
    else if (d_state == CONSUMING) {
        BALL_LOG_INFO << "Cancelling consumer: " << d_tag
                      << " for queue: " << d_queue->name();
        method = rmqamqpt::BasicCancel(d_tag);
    }
    else {
        BALL_LOG_WARN
            << "Cancel called in a state other than CONSUMING. State: "
            << d_state << ", queue: " << queueName();
    }

    d_state = CANCELLING;
    return method;
}

void Consumer::cancelOk()
{
    if (d_state != CANCELLING && d_state != CANCELLING_BUT_RESUMING) {
        BALL_LOG_ERROR << "Received CancelOk without sending Cancel consumer: ["
                       << d_tag << "]";
    }

    if (d_state == CANCELLING_BUT_RESUMING) {
        d_state = NOT_CONSUMING;
    }
    else {
        d_state = CANCELLED;
    }
}

Consumer::~Consumer() {}

} // namespace rmqamqp
} // namespace BloombergLP