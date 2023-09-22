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

#include <rmqa_tracingproducerimpl.h>

#include <bdlf_placeholder.h>
#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqa {

namespace {
void callbackAndContext(const rmqp::Producer::ConfirmationCallback& callback,
                        bsl::shared_ptr<rmqp::ProducerTracing::Context> context,
                        const rmqt::Message& message,
                        const bsl::string& routingKey,
                        const rmqt::ConfirmResponse& response)
{
    callback(message, routingKey, response);
    context->response(response);
}

bsl::string extractExchangeName(const rmqt::ExchangeHandle& exchangeHandle)
{
    bsl::shared_ptr<rmqt::Exchange> exchange(exchangeHandle.lock());
    return exchange ? exchange->name() : "<expired exchange>";
}

} // namespace

TracingProducerImpl::Factory::Factory(
    const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqp::ProducerTracing>& tracing)
: d_endpoint(endpoint)
, d_tracing(tracing)
{
}

bsl::shared_ptr<ProducerImpl> TracingProducerImpl::Factory::create(
    uint16_t maxOutstandingConfirms,
    const rmqt::ExchangeHandle& exchange,
    const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
    bdlmt::ThreadPool& threadPool,
    rmqio::EventLoop& eventLoop) const
{
    return bsl::shared_ptr<TracingProducerImpl>(
        new TracingProducerImpl(maxOutstandingConfirms,
                                channel,
                                threadPool,
                                eventLoop,
                                extractExchangeName(exchange),
                                d_endpoint,
                                d_tracing));
}

TracingProducerImpl::TracingProducerImpl(
    uint16_t maxOutstandingConfirms,
    const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
    bdlmt::ThreadPool& threadPool,
    rmqio::EventLoop& eventLoop,
    const bsl::string& exchangeName,
    const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqp::ProducerTracing>& tracing)
: ProducerImpl(maxOutstandingConfirms, channel, threadPool, eventLoop)
, d_exchangeName(exchangeName)
, d_endpoint(endpoint)
, d_tracing(tracing)
{
}

rmqp::Producer::SendStatus TracingProducerImpl::send(
    const rmqt::Message& message,
    const bsl::string& routingKey,
    const rmqp::Producer::ConfirmationCallback& confirmCallback,
    const bsls::TimeInterval& timeout)
{
    rmqt::Message newMessage(message);
    // ideally we'd have move semantics on the message to avoid
    // copying the metadata note that this is not a deep copy of
    // the message payload
    bsl::shared_ptr<rmqp::ProducerTracing::Context> context =
        d_tracing->createAndTag(
            &(newMessage.properties()), routingKey, d_exchangeName, d_endpoint);

    return ProducerImpl::send(newMessage,
                              routingKey,
                              bdlf::BindUtil::bind(&callbackAndContext,
                                                   confirmCallback,
                                                   context,
                                                   bdlf::PlaceHolders::_1,
                                                   bdlf::PlaceHolders::_2,
                                                   bdlf::PlaceHolders::_3),
                              timeout);
}

rmqp::Producer::SendStatus TracingProducerImpl::trySend(
    const rmqt::Message& message,
    const bsl::string& routingKey,
    const rmqp::Producer::ConfirmationCallback& confirmCallback)
{
    rmqt::Message newMessage(message);
    // ideally we'd have move semantics on the message to avoid
    // copying the metadata note that this is not a deep copy of
    // the message payload
    bsl::shared_ptr<rmqp::ProducerTracing::Context> context =
        d_tracing->createAndTag(
            &(newMessage.properties()), routingKey, d_exchangeName, d_endpoint);

    return ProducerImpl::trySend(newMessage,
                                 routingKey,
                                 bdlf::BindUtil::bind(&callbackAndContext,
                                                      confirmCallback,
                                                      context,
                                                      bdlf::PlaceHolders::_1,
                                                      bdlf::PlaceHolders::_2,
                                                      bdlf::PlaceHolders::_3));
}

} // namespace rmqa
} // namespace BloombergLP
