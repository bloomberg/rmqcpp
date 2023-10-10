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

#include <rmqa_messageguard.h>

#include <rmqp_consumer.h>

#include <ball_log.h>
#include <bsl_exception.h>

namespace BloombergLP {
namespace rmqa {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.MESSAGEGUARD")
}

bslma::ManagedPtr<rmqa::MessageGuard>
MessageGuard::Factory::create(const rmqt::Message& message,
                              const rmqt::Envelope& envelope,
                              const MessageGuardCallback& ackCallback,
                              rmqp::Consumer* consumer) const
{
    return bslma::ManagedPtrUtil::makeManaged<rmqa::MessageGuard>(
        message, envelope, ackCallback, consumer);
}

MessageGuard::Factory::~Factory() {}

MessageGuard::MessageGuard(const rmqt::Message& message,
                           const rmqt::Envelope& envelope,
                           const MessageGuardCallback& ackCallback,
                           rmqp::Consumer* consumer)
: d_state(READY)
, d_message(message)
, d_envelope(envelope)
, d_ackCallback(ackCallback)
, d_consumer(consumer)
{
}

MessageGuard::MessageGuard(const MessageGuard& obj)
: d_state(obj.d_state)
, d_message(obj.d_message)
, d_envelope(obj.d_envelope)
, d_ackCallback(obj.d_ackCallback)
, d_consumer(obj.d_consumer)
{
    obj.d_state = TRANSFERRED;
}

MessageGuard& MessageGuard::operator=(const MessageGuard& other)
{
    if (&other == this)
        return *this;
    d_message     = other.d_message;
    d_envelope    = other.d_envelope;
    d_ackCallback = other.d_ackCallback;
    d_state       = other.d_state;
    d_consumer    = other.d_consumer;
    other.d_state = TRANSFERRED;
    return *this;
}

MessageGuard::~MessageGuard()
{
    if (d_state == READY) {
        BALL_LOG_ERROR << "Unacked message, explicitly nacking. Message guid: "
                       << d_message.guid()
                       << ", payload size: " << d_message.payloadSize();
        try {
            d_ackCallback(
                rmqt::ConsumerAck(d_envelope, rmqt::ConsumerAck::REQUEUE));
        }
        catch (bsl::exception& e) {
            BALL_LOG_ERROR
                << "Exception thrown when implicitly acking. Message guid: "
                << d_message.guid()
                << ", payload size: " << d_message.payloadSize() << "Exception "
                << e.what();
        }
        catch (...) {
            BALL_LOG_ERROR
                << "Exception thrown when implicitly acking. Message guid: "
                << d_message.guid()
                << ", payload size: " << d_message.payloadSize();
        }
    }
}

const rmqt::Message& MessageGuard::message() const { return d_message; }
const rmqt::Envelope& MessageGuard::envelope() const { return d_envelope; }
rmqp::Consumer* MessageGuard::consumer() const { return d_consumer; }

void MessageGuard::ack() { resolve(rmqt::ConsumerAck::ACK); }

void MessageGuard::nack(bool requeue)
{
    resolve(requeue ? rmqt::ConsumerAck::REQUEUE : rmqt::ConsumerAck::REJECT);
}

void MessageGuard::resolve(rmqt::ConsumerAck::Type ackOption)
{
    if (d_state == TRANSFERRED) {
        BALL_LOG_ERROR << "Resolving invalid guard. Message guid: "
                       << d_message;
        return;
    }
    if (d_state == RESOLVED) {
        BALL_LOG_ERROR << "Message has already been (n)acked. Message guid: "
                       << d_message;
        return;
    }
    d_state = RESOLVED;
    d_ackCallback(rmqt::ConsumerAck(d_envelope, ackOption));
}

rmqp::TransferrableMessageGuard MessageGuard::transferOwnership()
{
    if (state() != READY) {
        BALL_LOG_WARN
            << "transferOwnership called when already transferred/resolved";
    }
    return bsl::make_shared<rmqa::MessageGuard>(*this);
}

bsl::ostream& operator<<(bsl::ostream& os, const MessageGuard& mg)
{
    bsl::string state;
    switch (mg.d_state) {
        case MessageGuard::READY:
            state = "UNRESOLVED";
            break;
        case MessageGuard::TRANSFERRED:
            state = "TRANSFERRED";
            break;
        default:
            break;
    }
    return os << "MessageGuard [" << mg.d_message.guid() << "] " << state;
}

} // namespace rmqa
} // namespace BloombergLP
