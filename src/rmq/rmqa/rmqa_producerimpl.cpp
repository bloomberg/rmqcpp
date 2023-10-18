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

#include <rmqa_producerimpl.h>

#include <rmqamqp_sendchannel.h>
#include <rmqio_eventloop.h>
#include <rmqt_confirmresponse.h>
#include <rmqt_future.h>
#include <rmqt_message.h>
#include <rmqt_topologyupdate.h>

#include <bdlmt_threadpool.h>
#include <bslma_managedptr.h>
#include <bslmt_lockguard.h>
#include <bslmt_mutex.h>
#include <bsls_systemtime.h>
#include <bsls_timeinterval.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_utility.h>

namespace BloombergLP {
namespace rmqa {
namespace {

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.PRODUCERIMPL")

void actionConfirmOnThreadPool(
    const rmqt::Message& message,
    const bsl::string& routingKey,
    const rmqt::ConfirmResponse& confirmResponse,
    const bsl::shared_ptr<ProducerImpl::SharedState>& sharedState)
{
    bslmt::LockGuard<bslmt::Mutex> guard(&(sharedState->mutex));

    if (!(sharedState->isValid)) {
        BALL_LOG_ERROR << "Received publisher confirmation for message "
                       << message.guid() << " after closing the producer";
        return;
    }

    ProducerImpl::CallbackMap::iterator it =
        sharedState->callbackMap.find(message.guid());

    if (it == sharedState->callbackMap.end()) {
        BALL_LOG_FATAL
            << "Failed to find Producer callback to invoke for message: "
            << message.guid()
            << ". Received duplicate confirm? The outstanding "
               "message limit will likely be affected for the lifetime of this "
               "Producer instance.";
        return;
    }

    BALL_LOG_TRACE << confirmResponse << " for " << message;

    sharedState->outstandingMessagesCap.post();

    it->second(message, routingKey, confirmResponse);

    sharedState->callbackMap.erase(it);

    if (sharedState->callbackMap.size() == 0 &&
        sharedState->waitForConfirmsFuture) {
        sharedState->waitForConfirmsFuture->first(rmqt::Result<>());
        sharedState->waitForConfirmsFuture.reset();
    }
}

void handleConfirmOnEventLoop(
    const bsl::shared_ptr<ProducerImpl::SharedState>& sharedState,
    const rmqt::Message& message,
    const bsl::string& routingKey,
    const rmqt::ConfirmResponse& confirmResponse)
{
    int rc = sharedState->threadPool.enqueueJob(
        bdlf::BindUtil::bind(&actionConfirmOnThreadPool,
                             message,
                             routingKey,
                             confirmResponse,
                             sharedState));

    if (rc != 0) {
        BALL_LOG_FATAL
            << "Couldn't enqueue thread pool job for message confirm: "
            << message.guid() << " (return code " << rc
            << "). Application will NEVER be informed of confirm";
    }
}

} // namespace

ProducerImpl::Factory::~Factory() {}

bsl::shared_ptr<ProducerImpl> ProducerImpl::Factory::create(
    uint16_t maxOutstandingConfirms,
    const rmqt::ExchangeHandle&,
    const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
    bdlmt::ThreadPool& threadPool,
    rmqio::EventLoop& eventLoop) const
{
    return bsl::shared_ptr<ProducerImpl>(new ProducerImpl(
        maxOutstandingConfirms, channel, threadPool, eventLoop));
}

ProducerImpl::ProducerImpl(uint16_t maxOutstandingConfirms,
                           const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
                           bdlmt::ThreadPool& threadPool,
                           rmqio::EventLoop& eventLoop)
: d_eventLoop(eventLoop)
, d_channel(channel)
, d_sharedState(bsl::shared_ptr<SharedState>(
      new SharedState(true, threadPool, maxOutstandingConfirms)))
{
    using namespace bdlf::PlaceHolders;
    channel->setCallback(bdlf::BindUtil::bind(
        &handleConfirmOnEventLoop, d_sharedState, _1, _2, _3));
}

ProducerImpl::~ProducerImpl()
{
    // Invalidate the callback passed to the channel so that it doesn't trigger
    // any client callbacks
    bslmt::LockGuard<bslmt::Mutex> guard(&(d_sharedState->mutex));
    d_sharedState->isValid = false;

    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::Channel::gracefulClose, d_channel));
}

bool ProducerImpl::registerUniqueCallback(
    const bdlb::Guid& guid,
    const rmqp::Producer::ConfirmationCallback& confirmCallback)
{
    bslmt::LockGuard<bslmt::Mutex> guard(&(d_sharedState->mutex));

    bsl::pair<ProducerImpl::CallbackMap::iterator, bool> result =
        d_sharedState->callbackMap.insert(
            bsl::make_pair(guid, confirmCallback));

    if (!result.second) {
        BALL_LOG_ERROR << "Cannot send message. Encountered duplicate "
                          "outstanding message GUID: "
                       << guid;

        return false;
    }

    return true;
}

rmqp::Producer::SendStatus
ProducerImpl::send(const rmqt::Message& message,
                   const bsl::string& routingKey,
                   const rmqp::Producer::ConfirmationCallback& confirmCallback,
                   const bsls::TimeInterval& timeout)
{
    // For safe delivery it's important the default mandatory flag value is
    // RETURN_UNROUTABLE
    rmqt::Mandatory::Value defaultMandatoryFlag =
        rmqt::Mandatory::RETURN_UNROUTABLE;

    return sendImpl(
        message, routingKey, defaultMandatoryFlag, confirmCallback, timeout);
}

rmqp::Producer::SendStatus
ProducerImpl::send(const rmqt::Message& message,
                   const bsl::string& routingKey,
                   rmqt::Mandatory::Value mandatoryFlag,
                   const rmqp::Producer::ConfirmationCallback& confirmCallback,
                   const bsls::TimeInterval& timeout)
{
    return sendImpl(
        message, routingKey, mandatoryFlag, confirmCallback, timeout);
}

rmqp::Producer::SendStatus ProducerImpl::sendImpl(
    const rmqt::Message& message,
    const bsl::string& routingKey,
    rmqt::Mandatory::Value mandatoryFlag,
    const rmqp::Producer::ConfirmationCallback& confirmCallback,
    const bsls::TimeInterval& timeout)
{
    BALL_LOG_TRACE
        << "Waiting on send(exchange) outstanding message limit for message "
        << message;

    if (timeout.totalNanoseconds()) {
        if (d_sharedState->outstandingMessagesCap.timedWait(
                bsls::SystemTime::nowRealtimeClock() + timeout)) {
            return rmqp::Producer::TIMEOUT;
        }
    }
    else {
        d_sharedState->outstandingMessagesCap.wait();
    }

    return doSend(message, routingKey, mandatoryFlag, confirmCallback);
}

rmqp::Producer::SendStatus ProducerImpl::trySend(
    const rmqt::Message& message,
    const bsl::string& routingKey,
    const rmqp::Producer::ConfirmationCallback& confirmCallback)
{
    if (!d_sharedState->outstandingMessagesCap.tryWait()) {
        return doSend(message,
                      routingKey,
                      rmqt::Mandatory::RETURN_UNROUTABLE,
                      confirmCallback);
    }
    else {
        BALL_LOG_TRACE << "Unconfirmed message limit already reached";
        return rmqp::Producer::INFLIGHT_LIMIT;
    }
}

rmqt::Future<>
ProducerImpl::updateTopologyAsync(const rmqt::TopologyUpdate& topologyUpdate)
{
    return rmqt::FutureUtil::flatten<void>(
        d_eventLoop.postF<rmqt::Future<> >(bdlf::BindUtil::bind(
            &rmqamqp::SendChannel::updateTopology, d_channel, topologyUpdate)));
}

rmqp::Producer::SendStatus ProducerImpl::doSend(
    const rmqt::Message& message,
    const bsl::string& routingKey,
    const rmqt::Mandatory::Value mandatory,
    const rmqp::Producer::ConfirmationCallback& confirmCallback)
{
    BALL_LOG_TRACE << "Below confirm limit";

    if (!registerUniqueCallback(message.guid(), confirmCallback)) {
        return rmqp::Producer::DUPLICATE;
    }

    d_eventLoop.post(bdlf::BindUtil::bind(&rmqamqp::SendChannel::publishMessage,
                                          d_channel,
                                          message,
                                          routingKey,
                                          mandatory));

    return rmqp::Producer::SENDING;
}

rmqt::Result<> ProducerImpl::waitForConfirms(const bsls::TimeInterval& timeout)
{
    rmqt::Result<> result;
    bool outstandingConfirms = false;
    bsl::optional<rmqt::Future<> > waitForConfirmsFuture;

    {
        bslmt::LockGuard<bslmt::Mutex> guard(&(d_sharedState->mutex));
        if (d_sharedState->callbackMap.size() > 0) {
            outstandingConfirms = true;
            if (d_sharedState->waitForConfirmsFuture) {
                waitForConfirmsFuture =
                    d_sharedState->waitForConfirmsFuture->second;
            }
            else {
                d_sharedState->waitForConfirmsFuture = rmqt::Future<>::make();
                waitForConfirmsFuture =
                    d_sharedState->waitForConfirmsFuture->second;
            }
        }
    }

    if (outstandingConfirms) {
        if (timeout.totalNanoseconds() == 0) {
            waitForConfirmsFuture->blockResult();
        }
        else {
            result = waitForConfirmsFuture->waitResult(timeout);
        }
    }

    return result;
}

} // namespace rmqa
} // namespace BloombergLP
