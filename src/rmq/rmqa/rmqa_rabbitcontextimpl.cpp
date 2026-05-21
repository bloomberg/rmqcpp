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

#include <rmqa_rabbitcontextimpl.h>

#include <rmqa_connectionimpl.h>
#include <rmqa_consumerimpl.h>
#include <rmqa_noopmetricpublisher.h>
#include <rmqa_producerimpl.h>
#include <rmqa_tracingconsumerimpl.h>
#include <rmqa_tracingproducerimpl.h>
#include <rmqa_vhost.h>
#include <rmqa_vhostimpl.h>

#include <rmqamqp_connection.h>
#include <rmqio_eventloop.h>
#include <rmqio_timer.h>
#include <rmqio_watchdog.h>
#include <rmqp_connection.h>
#include <rmqt_endpoint.h>
#include <rmqt_future.h>
#include <rmqt_vhostinfo.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bdlmt_threadpool.h>
#include <bsls_review.h>
#include <bsls_timeinterval.h>

#include <boost/algorithm/string/join.hpp>

#include <bsl_sstream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqa {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.RABBITCONTEXTIMPL")

const char DEFAULT_THREADPOOL_WORKER_NAME[] = "LIBRMQ.WORKER";

const int DEFAULT_THREADPOOL_MINTHREADS    = 2;
const int DEFAULT_THREADPOOL_MAXTHREADS    = 10;
const int DEFAULT_THREADPOOL_MAXIDLETIMEMS = 60 * 1000;

void handleErrorCbOnEventLoop(bdlmt::ThreadPool* threadPool,
                              const rmqt::ErrorCallback& errorCb,
                              const bsl::string& errorText,
                              int errorCode)
{
    if (errorCb) {
        threadPool->enqueueJob(
            bdlf::BindUtil::bind(errorCb, errorText, errorCode));
    }
}

void handleSuccessCbOnEventLoop(bdlmt::ThreadPool* threadPool,
                                const rmqt::SuccessCallback& successCb)
{
    if (successCb) {
        threadPool->enqueueJob(
            bdlf::BindUtil::bind(successCb));
    }
}

void startFirstConnection(
    const bsl::weak_ptr<rmqamqp::Connection>& weakConn,
    const rmqamqp::Connection::ConnectedCallback& callback)
{
    bsl::shared_ptr<rmqamqp::Connection> amqpConn = weakConn.lock();
    if (!amqpConn) {
        BALL_LOG_DEBUG << "Started connection as Future was destructed";
        return;
    }

    amqpConn->startFirstConnection(callback);
}

void initiateConnection(
    bool success,
    const bsl::weak_ptr<rmqamqp::Connection>& weakConn,
    const rmqt::Future<rmqp::Connection>::Maker& resolveFuture,
    rmqio::EventLoop& eventLoop,
    bdlmt::ThreadPool& threadPool,
    const rmqt::ErrorCallback& errorCb,
    const rmqt::SuccessCallback& successCb,
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const rmqt::Tunables& tunables,
    const bsl::shared_ptr<rmqa::ConsumerImpl::Factory>& consumerFactory,
    const bsl::shared_ptr<rmqa::ProducerImpl::Factory>& producerFactory)
{
    bsl::shared_ptr<rmqamqp::Connection> amqpConn = weakConn.lock();
    if (!amqpConn) {
        BALL_LOG_DEBUG << "Started connection as Future was destructed";
        return;
    }

    if (!success) {
        resolveFuture(
            rmqt::Result<rmqp::Connection>("Failed to connect - not retrying"));
        return;
    }

    resolveFuture(
        rmqt::Result<rmqp::Connection>(ConnectionImpl::make(amqpConn,
                                                            eventLoop,
                                                            threadPool,
                                                            errorCb,
                                                            successCb,
                                                            endpoint,
                                                            tunables,
                                                            consumerFactory,
                                                            producerFactory)));
}

void noopConnectionShutdown() {}

void shutdownAmqpConnection(
    rmqio::EventLoop& eventLoop,
    const bsl::shared_ptr<rmqamqp::Connection>& connection)
{
    // If we get here, the connection creation was cancelled.
    // This isn't going to be a graceful shutdown, but we will
    // send the close frame before shutting down the TCP socket.
    eventLoop.post(bdlf::BindUtil::bind(
        &rmqamqp::Connection::close, connection, &noopConnectionShutdown));
}

void asyncFetchConnectionInfo(
    const bsl::shared_ptr<rmqa::ConnectionMonitor>& monitor,
    rmqt::Future<ConnectionMonitor::AliveConnectionInfo>::Maker maker)
{
    maker(rmqt::Result<ConnectionMonitor::AliveConnectionInfo>(
        monitor->fetchAliveConnectionInfo()));
}
} // namespace

RabbitContextImpl::RabbitContextImpl(
    bslma::ManagedPtr<rmqio::EventLoop> eventLoop,
    const rmqa::RabbitContextOptions& options)
: d_eventLoop(eventLoop)
, d_watchDog(bsl::make_shared<rmqio::WatchDog>(
      bsls::TimeInterval(DEFAULT_WATCHDOG_PERIOD)))
, d_threadPool(options.threadpool())
, d_hostedThreadPool()
, d_onError(bdlf::BindUtil::bind(&handleErrorCbOnEventLoop,
                                 bsl::ref(d_threadPool),
                                 options.errorCallback(),
                                 bdlf::PlaceHolders::_1,
                                 bdlf::PlaceHolders::_2))
, d_onSuccess(bdlf::BindUtil::bind(&handleSuccessCbOnEventLoop,
                                 bsl::ref(d_threadPool),
                                 options.successCallback()))
, d_connectionMonitor(
      bsl::make_shared<ConnectionMonitor>(options.messageProcessingTimeout()))
, d_connectionFactory()
, d_tunables(options.tunables())
, d_consumerTracing(options.consumerTracing())
, d_producerTracing(options.producerTracing())
{
    bsl::shared_ptr<rmqp::MetricPublisher> metricPublisher =
        options.metricPublisher();
    if (!metricPublisher) {
        metricPublisher = bsl::make_shared<NoOpMetricPublisher>();
    }
    d_connectionFactory =
        bslma::ManagedPtrUtil::makeManaged<rmqamqp::Connection::Factory>(
            d_eventLoop->resolver(
                options.shuffleConnectionEndpoints().value_or(false)),
            d_eventLoop->timerFactory(),
            d_onError,
            d_onSuccess,
            metricPublisher,
            d_connectionMonitor,
            options.clientProperties(),
            options.connectionErrorThreshold());
    if (!d_threadPool) {
        bslmt::ThreadAttributes attributes;
        attributes.setThreadName(DEFAULT_THREADPOOL_WORKER_NAME);
        d_hostedThreadPool =
            bslma::ManagedPtrUtil::makeManaged<bdlmt::ThreadPool>(
                attributes,
                DEFAULT_THREADPOOL_MINTHREADS,
                DEFAULT_THREADPOOL_MAXTHREADS,
                DEFAULT_THREADPOOL_MAXIDLETIMEMS);
        d_hostedThreadPool->start();
        d_threadPool = d_hostedThreadPool.ptr();
    }
    BSLS_REVIEW(d_threadPool->enabled());

    d_eventLoop->start();
    d_watchDog->addTask(bsl::weak_ptr<ConnectionMonitor>(d_connectionMonitor));
    d_watchDog->start(d_eventLoop->timerFactory());
}

RabbitContextImpl::~RabbitContextImpl()
{
    rmqt::Future<ConnectionMonitor::AliveConnectionInfo>::Pair logOnEventLoop =
        rmqt::Future<ConnectionMonitor::AliveConnectionInfo>::make();

    d_producerTracing.reset();
    d_consumerTracing.reset();

    d_connectionFactory.reset();

    d_hostedThreadPool.reset();

    d_watchDog.reset();

    const bool eventLoopShutdownResult = d_eventLoop->waitForEventLoopExit(60);

    if (!eventLoopShutdownResult) {
        d_eventLoop->post(bdlf::BindUtil::bind(&asyncFetchConnectionInfo,
                                               d_connectionMonitor,
                                               logOnEventLoop.first));

        const int64_t MAX_CONN_INFO_WAIT_SEC = 60;

        rmqt::Result<ConnectionMonitor::AliveConnectionInfo> result =
            logOnEventLoop.second.waitResult(
                bsls::TimeInterval(MAX_CONN_INFO_WAIT_SEC));

        if (!result) {
            BALL_LOG_FATAL
                << "Attempted to query alive connections on the Event Loop but "
                   "no response after "
                << MAX_CONN_INFO_WAIT_SEC
                << " seconds. Something is badly wrong with the event loop if "
                   "it can't process anything in this time.";
        }
        else {
            typedef bsl::vector<
                ConnectionMonitor::AliveConnectionInfo::ConnectionChannelsInfo>
                ConnChannelInfo;

            const ConnChannelInfo& info =
                result.value()->aliveConnectionChannelInfo;

            if (info.size() > 0) {
                bsl::stringstream logOutput;

                for (ConnChannelInfo::const_iterator it = info.begin();
                     it != info.end();
                     ++it) {

                    logOutput << " Connection [" << it->first
                              << "] is still alive. It has "
                              << it->second.size() << " producers/consumers";

                    if (it->second.size() > 0) {
                        bsl::string channelsDebugInfo =
                            boost::algorithm::join(it->second, ", ");

                        logOutput << ": [" << channelsDebugInfo << "]. ";
                    }
                }

                BALL_LOG_FATAL
                    << "Attempting to destruct RabbitContext with outstanding "
                       "Producer/Consumer references. These will hold up "
                       "RabbitContext shutdown. Waited 60 seconds. Destruct "
                       "these objects first: "
                    << logOutput.str();
            }
        }
    }

    d_eventLoop.reset();
}

bsl::shared_ptr<rmqp::Connection> RabbitContextImpl::createVHostConnection(
    const bsl::string& userDefinedName,
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqt::Credentials>& credentials)
{
    return bsl::make_shared<rmqa::VHostImpl>(
        bdlf::BindUtil::bind(&RabbitContextImpl::createNewConnection,
                             this,
                             userDefinedName,
                             endpoint,
                             credentials,
                             bdlf::PlaceHolders::_1));
}

bsl::shared_ptr<rmqp::Connection>
RabbitContextImpl::createVHostConnection(const bsl::string& userDefinedName,
                                         const rmqt::VHostInfo& vhostInfo)
{
    return bsl::make_shared<rmqa::VHostImpl>(
        bdlf::BindUtil::bind(&RabbitContextImpl::createNewConnection,
                             this,
                             userDefinedName,
                             vhostInfo.endpoint(),
                             vhostInfo.credentials(),
                             bdlf::PlaceHolders::_1));
}

rmqt::Future<rmqp::Connection> RabbitContextImpl::createNewConnection(
    const bsl::string& userDefinedName,
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqt::Credentials>& credentials,
    const bsl::string& suffix)
{
    bsl::string name =
        suffix.empty() ? userDefinedName : userDefinedName + "-" + suffix;

    using bdlf::PlaceHolders::_1;

    if (!endpoint) {
        rmqt::Future<rmqp::Connection>::Pair futurePair =
            rmqt::Future<rmqp::Connection>::make();
        futurePair.first(
            rmqt::Result<rmqp::Connection>("Provided endpoint is NULL"));

        return futurePair.second;
    }

    if (!credentials) {
        rmqt::Future<rmqp::Connection>::Pair futurePair =
            rmqt::Future<rmqp::Connection>::make();
        futurePair.first(
            rmqt::Result<rmqp::Connection>("Provided credentials is NULL"));

        return futurePair.second;
    }

    if (!d_eventLoop->isStarted()) {
        rmqt::Future<rmqp::Connection>::Pair futurePair =
            rmqt::Future<rmqp::Connection>::make();
        futurePair.first(rmqt::Result<rmqp::Connection>(
            "Event loop worker thread not started"));

        return futurePair.second;
    }

    bsl::shared_ptr<rmqamqp::Connection> amqpConn =
        d_connectionFactory->create(endpoint, credentials, name);

    // The cancel function is given `amqpConn` which is what keeps it alive
    // until the shared_ptr<rmqamqp::Connection> is retrieved in
    // initiateConnection
    rmqt::Future<rmqp::Connection>::Pair futurePair =
        rmqt::Future<rmqp::Connection>::make(bdlf::BindUtil::bind(
            &shutdownAmqpConnection, bsl::ref(*d_eventLoop), amqpConn));

    bsl::shared_ptr<ConsumerImpl::Factory> consumerFactory(
        d_consumerTracing
            ? bsl::shared_ptr<ConsumerImpl::Factory>(
                  new TracingConsumerImpl::Factory(endpoint, d_consumerTracing))
            : bsl::make_shared<rmqa::ConsumerImpl::Factory>());

    bsl::shared_ptr<ProducerImpl::Factory> producerFactory(
        d_producerTracing
            ? bsl::shared_ptr<ProducerImpl::Factory>(
                  new TracingProducerImpl::Factory(endpoint, d_producerTracing))
            : bsl::make_shared<ProducerImpl::Factory>());

    rmqamqp::Connection::ConnectedCallback cb =
        bdlf::BindUtil::bind(&initiateConnection,
                             _1,
                             bsl::weak_ptr<rmqamqp::Connection>(amqpConn),
                             futurePair.first,
                             bsl::ref(*d_eventLoop),
                             bsl::ref(*d_threadPool),
                             d_onError,
                             d_onSuccess,
                             endpoint,
                             d_tunables,
                             consumerFactory,
                             producerFactory);

    d_eventLoop->post(
        bdlf::BindUtil::bind(&startFirstConnection,
                             bsl::weak_ptr<rmqamqp::Connection>(amqpConn),
                             cb));

    return futurePair.second;
}

} // namespace rmqa
} // namespace BloombergLP
