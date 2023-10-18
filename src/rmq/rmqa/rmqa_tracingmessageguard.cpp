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

#include <rmqa_tracingmessageguard.h>

#include <ball_log.h>
#include <bsl_exception.h>
#include <bsl_memory.h>
#include <bslmf_movableref.h>

namespace BloombergLP {
namespace rmqa {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.TRACINGMESSAGEGUARD")

bsl::string extractQueueName(const rmqt::QueueHandle& queueHandle)
{
    bsl::shared_ptr<rmqt::Queue> queue(queueHandle.lock());
    return queue ? queue->name() : "<expired queue>";
}
} // namespace

TracingMessageGuard::Factory::Factory(
    const rmqt::QueueHandle& queue,
    const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqp::ConsumerTracing>& contextFactory)
: d_queueName(extractQueueName(queue))
, d_endpoint(endpoint)
, d_contextFactory(contextFactory)
{
}

bslma::ManagedPtr<rmqa::MessageGuard>
TracingMessageGuard::Factory::create(const rmqt::Message& message,
                                     const rmqt::Envelope& envelope,
                                     const MessageGuardCallback& ackCallback,
                                     rmqp::Consumer* consumer) const
{
    return bslma::ManagedPtr<rmqa::MessageGuard>(
        new TracingMessageGuard(message,
                                envelope,
                                ackCallback,
                                consumer,
                                d_queueName,
                                d_endpoint,
                                d_contextFactory));
}

TracingMessageGuard::TracingMessageGuard(
    const rmqt::Message& message,
    const rmqt::Envelope& envelope,
    const MessageGuardCallback& ackCallback,
    rmqp::Consumer* consumer,
    const bsl::string& queueName,
    const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqp::ConsumerTracing>& contextFactory)
: rmqa::MessageGuard(message, envelope, ackCallback, consumer)
, d_context(contextFactory->create(*this, queueName, bsl::ref(endpoint)))
{
}

TracingMessageGuard::TracingMessageGuard(const TracingMessageGuard& obj)
: MessageGuard(obj)
, d_context(obj.d_context)
{
}

rmqp::TransferrableMessageGuard TracingMessageGuard::transferOwnership()
{
    if (state() != READY) {
        BALL_LOG_WARN
            << "transferOwnership called when already transferred/resolved";
    }
    return bsl::make_shared<rmqa::TracingMessageGuard>(*this);
}

} // namespace rmqa
} // namespace BloombergLP
