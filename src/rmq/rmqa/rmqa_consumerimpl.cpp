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

#include <rmqa_consumerimpl.h>

#include <rmqa_messageguard.h>
#include <rmqamqp_receivechannel.h>
#include <rmqio_eventloop.h>
#include <rmqp_consumer.h>
#include <rmqp_messageguard.h>
#include <rmqp_messagetransformer.h>
#include <rmqt_envelope.h>
#include <rmqt_message.h>
#include <rmqt_queue.h>
#include <rmqt_result.h>
#include <rmqt_topologyupdate.h>

#include <ball_log.h>

#include <bdlf_bind.h>
#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_vector.h>
#include <bslmt_lockguard.h>
#include <bslmt_mutex.h>

namespace BloombergLP {
namespace rmqa {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.CONSUMERIMPL")

} // namespace

ConsumerImpl::Factory::~Factory() {}

bsl::shared_ptr<ConsumerImpl> ConsumerImpl::Factory::create(
    const bsl::shared_ptr<rmqamqp::ReceiveChannel>& channel,
    rmqt::QueueHandle queue,
    const bsl::shared_ptr<ConsumerFunc>& onMessage,
    const bsl::string& consumerTag,
    bdlmt::ThreadPool& threadPool,
    rmqio::EventLoop& eventLoop,
    const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
    const bsl::vector<bsl::shared_ptr<rmqp::MessageTransformer> >& transformers)
    const
{
    return bsl::shared_ptr<ConsumerImpl>(
        new ConsumerImpl(channel,
                         queue,
                         onMessage,
                         consumerTag,
                         threadPool,
                         eventLoop,
                         ackQueue,
                         bsl::make_shared<rmqa::MessageGuard::Factory>(),
                         transformers));
}

ConsumerImpl::ConsumerImpl(
    const bsl::shared_ptr<rmqamqp::ReceiveChannel>& channel,
    rmqt::QueueHandle queue,
    const bsl::shared_ptr<rmqp::Consumer::ConsumerFunc>& onMessage,
    const bsl::string& consumerTag,
    bdlmt::ThreadPool& threadPool,
    rmqio::EventLoop& eventLoop,
    const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
    const bsl::shared_ptr<rmqa::MessageGuard::Factory>& guardFactory,
    const bsl::vector<bsl::shared_ptr<rmqp::MessageTransformer> >& transformers)
: d_consumerTag(consumerTag)
, d_queue(queue)
, d_onMessage(onMessage)
, d_threadPool(threadPool)
, d_eventLoop(eventLoop)
, d_ackQueue(ackQueue)
, d_ackBatch()
, d_ackMessageMutex()
, d_channel(channel)
, d_guardFactory(guardFactory)
, d_transformers(transformers)
, d_onNewAckBatch(
      bdlf::BindUtil::bind(&rmqamqp::ReceiveChannel::consumeAckBatchFromQueue,
                           d_channel))
, d_messageGuardCb()
{
}

ConsumerImpl::~ConsumerImpl()
{
    BALL_LOG_INFO << "Consumer shutting down: " << d_channel->inFlight()
                  << " messages in flight";

    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::Channel::gracefulClose, d_channel));
}

rmqt::Result<> ConsumerImpl::start()
{
    rmqt::Result<> result =
        d_channel->consume(d_queue,
                           bdlf::BindUtil::bind(&ConsumerImpl::handleMessage,
                                                weak_from_this(),
                                                bsl::ref(d_threadPool),
                                                bdlf::PlaceHolders::_1,
                                                bdlf::PlaceHolders::_2),
                           d_consumerTag);

    d_messageGuardCb = bdlf::BindUtil::bind(&ConsumerImpl::messageGuardCb,
                                            weak_from_this(),
                                            bdlf::PlaceHolders::_1);

    return result;
}

void ConsumerImpl::handleMessage(
    const bsl::weak_ptr<ConsumerImpl>& consumerWeakPtr,
    bdlmt::ThreadPool& threadPool,
    const rmqt::Message& message,
    const rmqt::Envelope& envelope)
{
    using bdlf::PlaceHolders::_1;

    int rc = threadPool.enqueueJob(bdlf::BindUtil::bind(
        &threadPoolHandleMessage, consumerWeakPtr, message, envelope));

    if (rc != 0) {
        BALL_LOG_ERROR << "Couldn't enqueue thread pool job for message "
                       << message.guid() << " (return code " << rc
                       << "). This message will NEVER be delivered to the "
                          "application and won't ever be acknowledged.";
    }
}

rmqt::Future<> ConsumerImpl::cancel()
{
    return rmqt::FutureUtil::flatten<void>(d_eventLoop.postF<rmqt::Future<> >(
        bdlf::BindUtil::bind(&rmqamqp::ReceiveChannel::cancel, d_channel)));
}

rmqt::Future<> ConsumerImpl::drain()
{
    return rmqt::FutureUtil::flatten<void>(d_eventLoop.postF<rmqt::Future<> >(
        bdlf::BindUtil::bind(&rmqamqp::ReceiveChannel::drain, d_channel)));
}

rmqt::Result<> ConsumerImpl::cancelAndDrain(const bsls::TimeInterval& timeout)
{
    bsl::function<rmqt::Future<>()> fn =
        bdlf::BindUtil::bind(&rmqp::Consumer::drain, this);
    rmqt::Future<> done =
        cancel().thenFuture<void>(rmqt::FutureUtil::propagateError<void>(fn));
    return timeout == bsls::TimeInterval(0) ? done.blockResult()
                                            : done.waitResult(timeout);
}

rmqt::Future<>
ConsumerImpl::updateTopologyAsync(const rmqt::TopologyUpdate& topologyUpdate)
{
    return rmqt::FutureUtil::flatten<void>(d_eventLoop.postF<rmqt::Future<> >(
        bdlf::BindUtil::bind(&rmqamqp::ReceiveChannel::updateTopology,
                             d_channel,
                             topologyUpdate)));
}

void ConsumerImpl::ackMessage(const rmqt::ConsumerAck& ack)
{
    // this method is executed by consumer threadpool workers and needs to be
    // thread-safe
    bslmt::LockGuard<bslmt::Mutex> guard(&d_ackMessageMutex);

    if (!d_ackBatch || !d_ackBatch->addAck(ack)) {
        d_ackBatch = bsl::make_shared<rmqt::ConsumerAckBatch>();
        if (!d_ackBatch->addAck(ack)) {
            BALL_LOG_ERROR << "Newly-created consumer ack batch already locked "
                           << ack.envelope().deliveryTag();
        }
        d_ackQueue->pushBack(d_ackBatch);
        d_eventLoop.post(d_onNewAckBatch);
    }
}

bool ConsumerImpl::unpackTransformations(rmqt::Message& dstMessage,
                                         const rmqt::Message& srcMessage)
{
    // Unpack source message
    bsl::shared_ptr<bsl::vector<uint8_t> > rawData =
        bsl::make_shared<bsl::vector<uint8_t> >(srcMessage.payload(),
                                                srcMessage.payload() +
                                                    srcMessage.payloadSize());
    rmqt::Properties properties = srcMessage.properties();

    // Undo all transformations
    for (bsl::vector<
             bsl::shared_ptr<rmqp::MessageTransformer> >::reverse_iterator it =
             d_transformers.rbegin();
         it != d_transformers.rend();
         ++it) {
        bsl::string headerName = "sdk.transform." + (*it)->name();
        if (!properties.headers ||
            properties.headers->find(headerName) == properties.headers->end()) {
            BALL_LOG_DEBUG << "No transformation header found for "
                           << (*it)->name();
            continue; // No transformation header, skip
        }
        rmqt::Result<> r = (*it)->inverseTransform(rawData, properties);
        if (!r) {
            BALL_LOG_ERROR << "Inverse transformation failed: " << r.error();
            return false;
        }
        properties.headers->erase(headerName); // Remove transformation header
    }

    // Pack into destination message
    dstMessage = rmqt::Message(rawData, properties);
    return true;
}

void ConsumerImpl::threadPoolHandleMessage(
    const bsl::weak_ptr<ConsumerImpl>& consumerWeakPtr,
    const rmqt::Message& message,
    const rmqt::Envelope& envelope)
{
    bsl::shared_ptr<ConsumerImpl> consumer = consumerWeakPtr.lock();

    if (!consumer) {
        BALL_LOG_WARN << "Ignoring new message as Consumer is shutting down: "
                      << message << " " << envelope;
        return;
    }

    rmqt::Message untransformedMsg;
    if (consumer->d_transformers.size() > 0) {
        if (!consumer->unpackTransformations(untransformedMsg, message)) {
            BALL_LOG_ERROR << "Failed to undo transformations to message "
                           << message.guid();
            return;
        }
    }
    const rmqt::Message& realMsg =
        consumer->d_transformers.size() > 0 ? untransformedMsg : message;

    using bdlf::PlaceHolders::_1;

    bslma::ManagedPtr<rmqa::MessageGuard> guard(
        consumer->d_guardFactory->create(
            realMsg, envelope, consumer->d_messageGuardCb, consumer.ptr()));

    BALL_LOG_DEBUG << "Delivering: " << *guard << " to client";

    (*consumer->d_onMessage)(*guard);

    BALL_LOG_DEBUG << "Processed: " << *guard << " from client";
}

void ConsumerImpl::messageGuardCb(
    const bsl::weak_ptr<ConsumerImpl>& consumerPtr,
    const rmqt::ConsumerAck& ack)
{
    bsl::shared_ptr<ConsumerImpl> consumer = consumerPtr.lock();

    if (!consumer) {
        BALL_LOG_WARN << "Consumer has been destructed. " << ack.envelope();
        return;
    }

    consumer->ackMessage(ack);
}

} // namespace rmqa
} // namespace BloombergLP
