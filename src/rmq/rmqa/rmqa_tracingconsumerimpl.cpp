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

#include <rmqa_tracingconsumerimpl.h>

#include <bsl_memory.h>
#include <rmqa_tracingmessageguard.h>

namespace BloombergLP {

namespace rmqa {
TracingConsumerImpl::Factory::Factory(
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqp::ConsumerTracing>& tracing)
: d_endpoint(endpoint)
, d_tracing(tracing)
{
}

bsl::shared_ptr<ConsumerImpl> TracingConsumerImpl::Factory::create(
    const bsl::shared_ptr<rmqamqp::ReceiveChannel>& channel,
    rmqt::QueueHandle queue,
    const bsl::shared_ptr<rmqa::ConsumerImpl::ConsumerFunc>& onMessage,
    const bsl::string& consumerTag,
    bdlmt::ThreadPool& threadPool,
    rmqio::EventLoop& eventLoop,
    const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue) const
{
    return bsl::shared_ptr<ConsumerImpl>(
        new ConsumerImpl(channel,
                         queue,
                         onMessage,
                         consumerTag,
                         threadPool,
                         eventLoop,
                         ackQueue,
                         bsl::make_shared<rmqa::TracingMessageGuard::Factory>(
                             queue, d_endpoint, d_tracing)));
}

} // namespace rmqa
} // namespace BloombergLP
