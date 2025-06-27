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

#include <rmqa_connectionimpl.h>

#include <rmqa_consumer.h>
#include <rmqa_consumerimpl.h>
#include <rmqa_producer.h>
#include <rmqa_producerimpl.h>

#include <rmqamqp_channel.h>
#include <rmqamqp_connection.h>
#include <rmqamqp_receivechannel.h>
#include <rmqamqp_sendchannel.h>

#include <rmqio_backofflevelretrystrategy.h>
#include <rmqio_timer.h>

#include <rmqt_consumerackbatch.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_properties.h>
#include <rmqt_result.h>

#include <ball_log.h>
#include <bdlf_bind.h>

#include <bsl_algorithm.h>
#include <bsl_iostream.h>
#include <bsl_limits.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqa {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.CONNECTIONIMPL")

bsl::shared_ptr<rmqio::RetryHandler>
createRetryHandler(const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory,
                   const rmqt::ErrorCallback& errorCb,
                   const rmqt::SuccessCallback& successCb,
                   const rmqt::Tunables& tunables)
{
    if (tunables.find("IIR") != tunables.end()) {
        bsl::cout << "WARNING: Infinite Immediate Retry Set"
                  << bsl::endl; // Intentional print to console
        return bsl::make_shared<rmqio::RetryHandler>(
            timerFactory,
            errorCb,
            successCb,
            bsl::make_shared<rmqio::BackoffLevelRetryStrategy>(
                bsl::numeric_limits<unsigned int>::max()));
    }
    return bsl::make_shared<rmqio::RetryHandler>(
        timerFactory,
        errorCb,
        successCb,
        bsl::make_shared<rmqio::BackoffLevelRetryStrategy>());
}

rmqt::Result<rmqp::Consumer> setupConsumer(
    const rmqt::QueueHandle& queue,
    const bsl::shared_ptr<rmqp::Consumer::ConsumerFunc>& onMessage,
    const bsl::string& consumerTag,
    rmqio::EventLoop& eventLoop,
    bdlmt::ThreadPool& threadPool,
    const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
    const bsl::shared_ptr<rmqa::ConsumerImpl::Factory>& consumerFactory,
    const rmqt::Result<rmqamqp::ReceiveChannel>& receiveChannel)
{
    if (receiveChannel) {
        bsl::shared_ptr<ConsumerImpl> consumer(
            consumerFactory->create(receiveChannel.value(),
                                    queue,
                                    onMessage,
                                    consumerTag,
                                    bsl::ref(threadPool),
                                    bsl::ref(eventLoop),
                                    ackQueue));
        rmqt::Result<> result = consumer->start();
        return result ? rmqt::Result<rmqp::Consumer>(consumer)
                      : rmqt::Result<rmqp::Consumer>(result.error(),
                                                     result.returnCode());
    }
    return rmqt::Result<rmqp::Consumer>(receiveChannel.error(),
                                        receiveChannel.returnCode());
}

rmqt::Result<rmqp::Producer> setupProducer(
    uint16_t maxOutstandingConfirms,
    const rmqt::ExchangeHandle& exchange,
    rmqio::EventLoop& eventLoop,
    bdlmt::ThreadPool& threadPool,
    const bsl::shared_ptr<rmqa::ProducerImpl::Factory>& producerFactory,
    const rmqt::Result<rmqamqp::SendChannel>& sendChannel)
{
    return sendChannel ? rmqt::Result<rmqp::Producer>(
                             producerFactory->create(maxOutstandingConfirms,
                                                     exchange,
                                                     sendChannel.value(),
                                                     bsl::ref(threadPool),
                                                     bsl::ref(eventLoop)))
                       : rmqt::Result<rmqp::Producer>(sendChannel.error(),
                                                      sendChannel.returnCode());
}

bool DoesExist(const bsl::shared_ptr<rmqt::Exchange>& element,
               const bsl::shared_ptr<rmqt::Exchange>& exchange)
{
    return *element == *exchange;
}
void noopTimerHandler(rmqio::Timer::InterruptReason) {}

void cancelCloseTimer(const bsl::weak_ptr<rmqio::Timer>& weakTimer)
{
    bsl::shared_ptr<rmqio::Timer> timer = weakTimer.lock();
    if (timer) {
        timer->resetCallback(&noopTimerHandler);
        timer->cancel();
    }
}

void gracefulCloseTimeout(const bsl::shared_ptr<rmqio::Timer>& timer,
                          const bsl::shared_ptr<rmqamqp::Connection>& conn,
                          rmqio::Timer::InterruptReason reason)
{
    (void)conn; // conn is passed as an argument to keep it alive. It needs to
                // live longer than this callback

    if (reason == rmqio::Timer::CANCEL) {
        BALL_LOG_TRACE << "Connection gracefully closed to broker: "
                       << conn->connectionDebugName();
    }
    else {
        BALL_LOG_INFO << "Timed out waiting for graceful close from broker: "
                      << conn->connectionDebugName();
    }

    timer->resetCallback(&noopTimerHandler);
}
const static uint16_t GRACEFUL_CLOSE_TIMEOUT_SEC = 5;
} // namespace

ConnectionImpl::ConnectionImpl(
    const bsl::shared_ptr<rmqamqp::Connection>& amqpConn,
    rmqio::EventLoop& loop,
    bdlmt::ThreadPool& threadPool,
    const rmqt::ErrorCallback& errorCb,
    const rmqt::SuccessCallback& successCb,
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const rmqt::Tunables& tunables,
    const bsl::shared_ptr<rmqa::ConsumerImpl::Factory>& consumerFactory,
    const bsl::shared_ptr<rmqa::ProducerImpl::Factory>& producerFactory)
: d_tunables(tunables)
, d_connection(amqpConn)
, d_threadPool(threadPool)
, d_eventLoop(loop)
, d_onError(errorCb)
, d_onSuccess(successCb)
, d_endpoint(endpoint)
, d_consumerFactory(consumerFactory)
, d_producerFactory(producerFactory)
{
}

void ConnectionImpl::close() { doClose(); }

void ConnectionImpl::doClose()
{
    if (d_connection) {
        BALL_LOG_TRACE << "Setting up graceful close timer for connection: "
                       << d_connection->connectionDebugName();

        // Setup a timer object, and trigger this to execute in a few seconds
        bsl::shared_ptr<rmqio::Timer> timer =
            d_eventLoop.timerFactory()->createWithTimeout(
                bsls::TimeInterval(GRACEFUL_CLOSE_TIMEOUT_SEC));

        rmqamqp::Connection::CloseFinishCallback close = bdlf::BindUtil::bind(
            &cancelCloseTimer, bsl::weak_ptr<rmqio::Timer>(timer));

        // This is awkward because it's holding onto itself, we mitigate the
        // risk of an ownership cycle by always dropping ownership to the timer
        // when the timer is invoked
        timer->start(bdlf::BindUtil::bind(&gracefulCloseTimeout,
                                          timer,
                                          d_connection,
                                          bdlf::PlaceHolders::_1));

        // Shutdown the connection
        d_eventLoop.post(bdlf::BindUtil::bind(
            &rmqamqp::Connection::close, d_connection, close));

        // The timer is the only thing keeping this connection alive now.
        d_connection.reset();
    }
}

bsl::shared_ptr<rmqa::ConnectionImpl> ConnectionImpl::make(
    const bsl::shared_ptr<rmqamqp::Connection>& amqpConn,
    rmqio::EventLoop& eventLoop,
    bdlmt::ThreadPool& threadPool,
    const rmqt::ErrorCallback& errorCb,
    const rmqt::SuccessCallback& successCb,
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const rmqt::Tunables& tunables,
    const bsl::shared_ptr<rmqa::ConsumerImpl::Factory>& consumerFactory,
    const bsl::shared_ptr<rmqa::ProducerImpl::Factory>& producerFactory)
{
    return bsl::shared_ptr<rmqa::ConnectionImpl>(
        new rmqa::ConnectionImpl(amqpConn,
                                 eventLoop,
                                 threadPool,
                                 errorCb,
                                 successCb,
                                 endpoint,
                                 tunables,
                                 consumerFactory,
                                 producerFactory));
}

rmqt::Result<rmqp::Producer>
ConnectionImpl::createProducer(const rmqt::Topology& topology,
                               rmqt::ExchangeHandle exchange,
                               uint16_t maxOutstandingConfirms)
{
    return createProducerAsync(topology, exchange, maxOutstandingConfirms)
        .blockResult();
}

rmqt::Result<rmqp::Consumer>
ConnectionImpl::createConsumer(const rmqt::Topology& topology,
                               rmqt::QueueHandle queue,
                               const rmqp::Consumer::ConsumerFunc& onMessage,
                               const rmqt::ConsumerConfig& consumerConfig)
{
    return createConsumerAsync(topology, queue, onMessage, consumerConfig)
        .blockResult();
}

rmqt::Future<rmqp::Producer>
ConnectionImpl::createProducerAsync(const rmqt::Topology& topology,
                                    rmqt::ExchangeHandle exchangeHandle,
                                    uint16_t maxOutstandingConfirms)
{
    rmqt::Future<rmqp::Producer>::Pair futurePair =
        rmqt::Future<rmqp::Producer>::make();

    bsl::shared_ptr<rmqt::Exchange> exchange = exchangeHandle.lock();

    if (!d_connection) {
        BALL_LOG_ERROR << "close() has been called";
        return rmqt::Future<rmqp::Producer>(
            rmqt::Result<rmqp::Producer>("close() has been called"));
    }

    if (!exchange) {
        BALL_LOG_ERROR << "Exchange passed to createProducer was destructed. "
                          "Caused by topology update?";
        return rmqt::Future<rmqp::Producer>(
            rmqt::Result<rmqp::Producer>("Exchange is not valid"));
    }

    const rmqt::Topology::ExchangeVec::const_iterator it = bsl::find_if(
        topology.exchanges.cbegin(),
        topology.exchanges.cend(),
        bdlf::BindUtil::bind(
            &DoesExist, bdlf::PlaceHolders::_1, bsl::ref(exchange)));

    if (it == topology.exchanges.cend()) {
        BALL_LOG_ERROR << "Passed Exchange " << exchange->name()
                       << " does not exist in the passed topology";

        return rmqt::Future<rmqp::Producer>(rmqt::Result<rmqp::Producer>(
            "Exchange does not exist in the topology"));
    }

    rmqt::Future<rmqamqp::SendChannel> sendChannelFuture(
        rmqt::FutureUtil::flatten<rmqamqp::SendChannel>(
            d_eventLoop.postF<rmqt::Future<rmqamqp::SendChannel> >(
                bdlf::BindUtil::bind(
                    &rmqamqp::Connection::createTopologySyncedSendChannel,
                    d_connection,
                    topology,
                    exchange,
                    createRetryHandler(d_eventLoop.timerFactory(),
                                       bsl::ref(d_onError),
                                       bsl::ref(d_onSuccess),
                                       d_tunables)))));

    return sendChannelFuture.then<rmqp::Producer>(
        bdlf::BindUtil::bind(&setupProducer,
                             maxOutstandingConfirms,
                             exchangeHandle,
                             bsl::ref(d_eventLoop),
                             bsl::ref(d_threadPool),
                             d_producerFactory,
                             bdlf::PlaceHolders::_1));
}

rmqt::Future<rmqp::Consumer> ConnectionImpl::createConsumerAsync(
    const rmqt::Topology& topology,
    rmqt::QueueHandle queue,
    const rmqp::Consumer::ConsumerFunc& onMessage,
    const rmqt::ConsumerConfig& consumerConfig)
{

    if (!d_connection) {
        BALL_LOG_ERROR << "close() has been called";

        return rmqt::Future<rmqp::Consumer>(
            rmqt::Result<rmqp::Consumer>("close() has been called"));
    }

    bsl::shared_ptr<rmqp::Consumer::ConsumerFunc> consumerFn =
        bsl::make_shared<rmqp::Consumer::ConsumerFunc>(onMessage);

    bsl::shared_ptr<rmqt::ConsumerAckQueue> ackQueue =
        bsl::make_shared<rmqt::ConsumerAckQueue>();

    rmqt::Future<rmqamqp::ReceiveChannel> receiveChannelFuture(
        rmqt::FutureUtil::flatten<rmqamqp::ReceiveChannel>(
            d_eventLoop.postF<rmqt::Future<rmqamqp::ReceiveChannel> >(
                bdlf::BindUtil::bind(
                    &rmqamqp::Connection::createTopologySyncedReceiveChannel,
                    d_connection,
                    topology,
                    consumerConfig,
                    createRetryHandler(d_eventLoop.timerFactory(),
                                       bsl::ref(d_onError),
                                       bsl::ref(d_onSuccess),
                                       d_tunables),
                    ackQueue))));

    return receiveChannelFuture.then<rmqp::Consumer>(bdlf::BindUtil::bind(
        &setupConsumer,
        queue,
        consumerFn,
        consumerConfig.consumerTag(),
        bsl::ref(d_eventLoop),
        consumerConfig.threadpool() ? bsl::ref(*consumerConfig.threadpool())
                                    : bsl::ref(d_threadPool),
        ackQueue,
        d_consumerFactory,
        bdlf::PlaceHolders::_1));
}

ConnectionImpl::~ConnectionImpl() { doClose(); }

} // namespace rmqa
} // namespace BloombergLP
