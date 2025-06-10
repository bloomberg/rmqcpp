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

#include <rmqamqp_connection.h>

#include <rmqamqp_framer.h>
#include <rmqamqp_heartbeatmanager.h>
#include <rmqamqp_heartbeatmanagerimpl.h>
#include <rmqamqp_message.h>
#include <rmqamqp_metrics.h>
#include <rmqamqpt_connectionopen.h>
#include <rmqamqpt_connectionopenok.h>
#include <rmqamqpt_connectionstart.h>
#include <rmqamqpt_connectionstartok.h>
#include <rmqamqpt_connectiontune.h>
#include <rmqamqpt_connectiontuneok.h>
#include <rmqamqpt_constants.h>
#include <rmqamqpt_frame.h>
#include <rmqamqpt_method.h>
#include <rmqio_backofflevelretrystrategy.h>
#include <rmqio_connection.h>
#include <rmqio_connectionretryhandler.h>
#include <rmqio_resolver.h>
#include <rmqio_retryhandler.h>
#include <rmqio_serializedframe.h>
#include <rmqio_timer.h>
#include <rmqp_metricpublisher.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_fieldvalue.h>

#include <bdlf_bind.h>
#include <bdlf_placeholder.h>
#include <bdlt_currenttime.h>
#include <bsl_algorithm.h>
#include <bsl_cstring.h>
#include <bsl_functional.h>
#include <bsl_limits.h>
#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {
namespace {

template <typename A, typename B>
rmqt::Result<A> passOnSuccessImpl(const bsl::shared_ptr<A>& item,
                                  const rmqt::Result<B>& result)
{
    return result ? rmqt::Result<A>(item)
                  : rmqt::Result<A>(result.error(), result.returnCode());
}

template <typename A>
bsl::function<rmqt::Result<A>(const rmqt::Result<void>&)>
passOnSuccess(const bsl::shared_ptr<A>& item)
{
    return bdlf::BindUtil::bind(
        &passOnSuccessImpl<A, void>, item, bdlf::PlaceHolders::_1);
}

const bsl::uint16_t k_MAX_CHANNEL_NUM = bsl::numeric_limits<uint16_t>::max();

// k_MAX_HEARTBEAT_TIMEOUT_SEC cannot be zero as the HeartbeatManager does
// not support this
const bsl::uint16_t k_MAX_HEARTBEAT_TIMEOUT_SEC = 60;
const bsl::uint16_t k_HUNG_TIMER_SEC            = 60;

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.CONNECTION")

rmqt::FieldTable generateClientProperties(const rmqt::FieldTable& base,
                                          const bsl::string& connectionName)
{
    using namespace rmqt;

    FieldTable props(base);
    bsl::shared_ptr<FieldTable> capabilities = bsl::make_shared<FieldTable>();

    (*capabilities)["connection.blocked"]           = FieldValue(false);
    (*capabilities)["authentication_failure_close"] = FieldValue(true);
    (*capabilities)["consumer_cancel_notify"]       = FieldValue(true);
    (*capabilities)["publisher_confirms"]           = FieldValue(true);
    (*capabilities)["basic.nack"]                   = FieldValue(false);

    props["capabilities"] = FieldValue(capabilities);

    props["platform"] = FieldValue(bsl::string(rmqamqpt::Constants::PLATFORM));
    props["product"]  = FieldValue(bsl::string(rmqamqpt::Constants::PRODUCT));
    props["version"]  = FieldValue(bsl::string(rmqamqpt::Constants::VERSION));

    if (!connectionName.empty()) {
        props["connection_name"] = FieldValue(connectionName);
    }

    return props;
}

template <typename Number>
Number negotiateTuneParam(Number server, Number client)
{
    if (server == 0) {
        return client;
    }

    if (client == 0) {
        return server;
    }

    return bsl::min(server, client);
}

void negotiateTuneParams(uint16_t* outChannelMax,
                         uint32_t* outFrameSizeMax,
                         uint16_t* outHeartbeat,
                         const rmqamqpt::ConnectionTune& serverTune,
                         const uint16_t clientChannelMax,
                         const uint32_t clientFrameSize,
                         const uint16_t clientHeartbeat)
{
    *outChannelMax =
        negotiateTuneParam(serverTune.channelMax(), clientChannelMax);
    *outFrameSizeMax =
        negotiateTuneParam(serverTune.frameMax(), clientFrameSize);
    *outHeartbeat =
        negotiateTuneParam(serverTune.heartbeatInterval(), clientHeartbeat);

    BALL_LOG_INFO << "Negotiated Tune Parameters "
                     "[Server:Client=Negotiated] channel-max["
                  << serverTune.channelMax() << ":" << clientChannelMax << "="
                  << *outChannelMax << "] frame-max [" << serverTune.frameMax()
                  << ":" << clientFrameSize << "=" << *outFrameSizeMax
                  << "] heartbeat-timeout [" << serverTune.heartbeatInterval()
                  << ":" << clientHeartbeat << "=" << *outHeartbeat << "]";
}

void noopWriteComplete() {}

bsl::shared_ptr<rmqio::SerializedFrame>
serializeFrame(const rmqamqpt::Frame& frame)
{
    return bsl::make_shared<rmqio::SerializedFrame>(frame);
}

} // namespace

Connection::Connection(
    const bsl::shared_ptr<rmqio::Resolver>& resolver,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
    const bsl::shared_ptr<rmqamqp::HeartbeatManager>& hbManager,
    const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory,
    const bsl::shared_ptr<rmqamqp::ChannelFactory>& channelFactory,
    const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqt::Credentials>& credentials,
    const rmqt::FieldTable& clientProperties,
    bsl::string_view name)
: d_resolver(resolver)
, d_retryHandler(retryHandler)
, d_heartbeatManager(hbManager)
, d_endpoint(endpoint)
, d_credentials(credentials)
, d_socketConnection()
, d_channelFactory(channelFactory)
, d_metricPublisher(metricPublisher)
, d_framer()
, d_state(Connection::DISCONNECTED)
, d_clientProperties(clientProperties)
, d_channels()
, d_hungTimer(
      timerFactory->createWithTimeout(bsls::TimeInterval(k_HUNG_TIMER_SEC)))
, d_timerFactory(timerFactory)
, d_firstConnectCb()
, d_closeCb()
, d_connectStartTime()
, d_hasBeenConnected(false)
, d_vhostTags()
, d_connectionName(name)
{
    d_vhostTags.push_back(bsl::pair<bsl::string, bsl::string>(
        Metrics::VHOST_TAG, d_endpoint->vhost()));
}

void Connection::close(const CloseFinishCallback& closeCallback)
{
    BALL_LOG_INFO << "Closing connection: " << connectionDebugName();

    d_closeCb = closeCallback;

    if (d_state == DISCONNECTED) {
        socketShutdown(Connection::FATAL,
                       rmqio::Connection::GRACEFUL_DISCONNECT);
    }
    else {
        sendConnectionClose(rmqamqpt::Constants::REPLY_SUCCESS,
                            "Closing Connection");
    }
}

Connection::~Connection()
{
    BALL_LOG_TRACE << "Destruct connection: " << connectionDebugName();
    socketShutdown(Connection::FATAL, rmqio::Connection::GRACEFUL_DISCONNECT);

    if (d_closeCb) {
        d_closeCb();
    }
}

void Connection::initiateConnect()
{
    using bdlf::PlaceHolders::_1;
    BALL_LOG_INFO << "Starting connection to: " << connectionDebugName();
    d_connectStartTime = bdlt::CurrentTime::now();
    rmqio::Connection::Callbacks callbacks;
    callbacks.onRead =
        bdlf::BindUtil::bind(&Connection::readHandler, weak_from_this(), _1);
    callbacks.onError =
        bdlf::BindUtil::bind(&Connection::socketError, weak_from_this(), _1);

    d_hungTimer->start(
        bdlf::BindUtil::bind(&Connection::connectionHung, this, _1));

    if (d_endpoint->securityParameters()) {
        d_socketConnection = d_resolver->asyncSecureConnect(
            d_endpoint->hostname(),
            d_endpoint->port(),
            rmqamqpt::Frame::getMaxFrameSize(),
            d_endpoint->securityParameters(),
            callbacks,
            bdlf::BindUtil::bind(&Connection::connectCb, weak_from_this()),
            bdlf::BindUtil::bind(
                &Connection::connectErrorCb, weak_from_this(), _1));
    }
    else {
        d_socketConnection = d_resolver->asyncConnect(
            d_endpoint->hostname(),
            d_endpoint->port(),
            rmqamqpt::Frame::getMaxFrameSize(),
            callbacks,
            bdlf::BindUtil::bind(&Connection::connectCb, weak_from_this()),
            bdlf::BindUtil::bind(
                &Connection::connectErrorCb, weak_from_this(), _1));
    }
}

void Connection::startFirstConnection(
    const ConnectedCallback& connectedCallback)
{
    d_firstConnectCb = connectedCallback;

    initiateConnect();
}

void Connection::connect()
{
    asyncWriteSingleFrame(
        bsl::make_shared<rmqio::SerializedFrame>(
            static_cast<const uint8_t*>(rmqamqpt::Constants::PROTOCOL_HEADER),
            rmqamqpt::Constants::PROTOCOL_HEADER_LENGTH),
        bdlf::BindUtil::bind(&Connection::onWriteCompleteCb,
                             weak_from_this(),
                             PROTOCOL_HEADER_SENT));
}

void Connection::connectError(const rmqio::Resolver::Error&)
{
    socketShutdown(WILL_RETRY, rmqio::Connection::CONNECT_ERROR);
}

void Connection::connectCb(const bsl::weak_ptr<Connection> weakSelf)
{
    bsl::shared_ptr<Connection> self = weakSelf.lock();

    if (!self) {
        BALL_LOG_DEBUG << "Connect came back after connection destructed";
        return;
    }

    self->connect();
}

void Connection::connectErrorCb(const bsl::weak_ptr<Connection> weakSelf,
                                const rmqio::Resolver::Error& error)
{
    bsl::shared_ptr<Connection> self = weakSelf.lock();

    if (!self) {
        BALL_LOG_DEBUG << "connectError came back after connection destructed";
        return;
    }

    self->connectError(error);
}

void Connection::socketError(const bsl::weak_ptr<Connection>& weakConn,
                             const rmqio::Connection::ReturnCode& rc)
{
    bsl::shared_ptr<Connection> conn = weakConn.lock();

    if (!conn) {
        // Connection shutdown
        BALL_LOG_TRACE << "Socket Error after Connection closed. RC: " << rc;
        return;
    }

    if (rc == rmqio::Connection::GRACEFUL_DISCONNECT) {
        return;
    }

    BALL_LOG_TRACE << "Socket snapped, RC: " << rc;

    conn->socketShutdown(WILL_RETRY, rc);
}

void Connection::onWriteComplete(State state)
{
    d_state = state;
    BALL_LOG_TRACE << "State now set to: " << d_state;

    // close socket if the server closed us
    if (d_state == CONNECTION_CLOSEOK_SENT) {
        closeSocket(WILL_RETRY);
    }
}

void Connection::onWriteCompleteCb(bsl::weak_ptr<Connection> weakSelf,
                                   State newState)
{
    bsl::shared_ptr<Connection> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_TRACE << "Write completion handler called during shutdown";
        return;
    }

    self->onWriteComplete(newState);
}

void Connection::sendConnectionStartOk()
{
    rmqamqpt::ConnectionStartOk startOkMethod(
        d_clientProperties,
        rmqamqpt::Constants::AUTHENTICATION_MECHANISM,
        d_credentials->formatCredentials(),
        rmqamqpt::Constants::LOCALE);

    const uint16_t channel = 0;
    rmqamqpt::Frame frame;
    Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::ConnectionMethod(startOkMethod));

    asyncWriteSingleFrame(serializeFrame(frame),
                          bdlf::BindUtil::bind(&Connection::onWriteCompleteCb,
                                               weak_from_this(),
                                               CONNECTION_STARTOK_SENT));
    BALL_LOG_TRACE << "Connection Start-ok method sent to server: "
                   << startOkMethod;
}

void Connection::sendConnectionTuneOk(
    const rmqamqpt::ConnectionTune& tuneMethod)
{
    bsl::uint16_t negotiatedChannelMax;
    bsl::uint32_t negotiatedMaxFrameSize;
    bsl::uint16_t negotiatedHeartbeatTimeout;

    negotiateTuneParams(
        &negotiatedChannelMax,
        &negotiatedMaxFrameSize,
        &negotiatedHeartbeatTimeout,
        tuneMethod,
        k_MAX_CHANNEL_NUM,
        static_cast<uint32_t>(rmqamqpt::Frame::getMaxFrameSize()),
        k_MAX_HEARTBEAT_TIMEOUT_SEC);

    startHeartbeatManager(negotiatedHeartbeatTimeout);
    d_framer.setMaxFrameSize(negotiatedMaxFrameSize);

    rmqamqpt::ConnectionTuneOk tuneOkMethod(negotiatedChannelMax,
                                            negotiatedMaxFrameSize,
                                            negotiatedHeartbeatTimeout);
    const uint16_t channel = 0;
    rmqamqpt::Frame frame;
    Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::ConnectionMethod(tuneOkMethod));
    asyncWriteSingleFrame(serializeFrame(frame),
                          bdlf::BindUtil::bind(&Connection::onWriteCompleteCb,
                                               weak_from_this(),
                                               CONNECTION_TUNEOK_SENT));
    BALL_LOG_TRACE << "Connection Tune-ok method sent to server: "
                   << tuneOkMethod;
}

void Connection::sendConnectionOpen()
{
    rmqamqpt::ConnectionOpen openMethod(d_endpoint->vhost());
    const uint16_t channel = 0;
    rmqamqpt::Frame frame;
    Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::ConnectionMethod(openMethod));
    asyncWriteSingleFrame(serializeFrame(frame),
                          bdlf::BindUtil::bind(&Connection::onWriteCompleteCb,
                                               weak_from_this(),
                                               CONNECTION_OPEN_SENT));
    BALL_LOG_TRACE << "Connection Open method sent to server: " << openMethod;
}

void Connection::sendConnectionCloseOk()
{
    rmqamqpt::ConnectionCloseOk closeOkMethod;
    const uint16_t channel = 0;
    rmqamqpt::Frame frame;
    Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::ConnectionMethod(closeOkMethod));
    asyncWriteSingleFrame(serializeFrame(frame),
                          bdlf::BindUtil::bind(&Connection::onWriteComplete,
                                               shared_from_this(),
                                               CONNECTION_CLOSEOK_SENT));
    BALL_LOG_TRACE << "Connection Close-ok method sent to server: "
                   << closeOkMethod;
}

void Connection::sendConnectionClose(
    rmqamqpt::Constants::AMQPReplyCode replyCode,
    const bsl::string& replyText,
    rmqamqpt::Constants::AMQPClassId failingClassId,
    rmqamqpt::Constants::AMQPMethodId failingMethodId)
{
    rmqamqpt::ConnectionClose closeMethod(
        replyCode, replyText, failingClassId, failingMethodId);
    const uint16_t channel = 0;
    rmqamqpt::Frame frame;
    Framer::makeMethodFrame(
        &frame, channel, rmqamqpt::ConnectionMethod(closeMethod));
    if (replyCode != rmqamqpt::Constants::REPLY_SUCCESS) {
        asyncWriteSingleFrame(
            serializeFrame(frame),
            bdlf::BindUtil::bind(&Connection::onWriteComplete,
                                 shared_from_this(),
                                 CONNECTION_CLOSE_SENT_RECONNECT));
    }
    else {
        asyncWriteSingleFrame(serializeFrame(frame),
                              bdlf::BindUtil::bind(&Connection::onWriteComplete,
                                                   shared_from_this(),
                                                   CONNECTION_CLOSE_SENT_EXIT));
    }

    BALL_LOG_TRACE << "Connection Close method sent to server: " << closeMethod;
}

void Connection::connectionException(
    rmqamqpt::Constants::AMQPReplyCode replyCode,
    const bsl::string& replyText,
    rmqamqpt::Constants::AMQPClassId failingClassId,
    rmqamqpt::Constants::AMQPMethodId failingMethodId)
{
    switch (d_state) {
        case CONNECTION_CLOSE_SENT_EXIT:
            closeSocket(FATAL);
            break;

        case CONNECTED:
        case PROTOCOL_HEADER_SENT:
        case CONNECTION_START_RECEIVED:
        case CONNECTION_STARTOK_SENT:
        case CONNECTION_TUNE_RECEIVED:
        case CONNECTION_TUNEOK_SENT:
        case CONNECTION_OPEN_SENT:
        case CONNECTION_OPENOK_RECEIVED:
            sendConnectionClose(
                replyCode, replyText, failingClassId, failingMethodId);
            break;
        default:
            closeSocket(WILL_RETRY);
            break;
    }
}

void Connection::retry(const bsl::weak_ptr<Connection>& weakSelf)
{
    bsl::shared_ptr<Connection> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_TRACE << "Retry handler called during shutdown";
        return;
    }
    self->initiateConnect();
}

void Connection::closeSocket(DisconnectType disconnectType)
{
    BALL_LOG_TRACE << "closeSocket: " << disconnectType;

    d_state = CONNECTION_CLOSED;
    BALL_LOG_TRACE << "State now set to: " << d_state;
    using bdlf::PlaceHolders::_1;
    if (d_socketConnection && d_socketConnection->isConnected()) {
        BALL_LOG_TRACE << "Closing Socket Gracefully";
        d_socketConnection->close(
            bdlf::BindUtil::bind(&Connection::socketShutdown,
                                 shared_from_this(),
                                 disconnectType,
                                 _1));
    }
    else {
        socketShutdown(disconnectType, rmqio::Connection::HUNG_ERROR);
    }
}

void Connection::socketShutdown(DisconnectType dcType,
                                rmqio::Connection::ReturnCode code)
{
    if (code != rmqio::Connection::CONNECT_ERROR) {
        BALL_LOG_INFO << "Disconnected from Broker: " << connectionDebugName()
                      << (dcType == rmqamqp::Connection::WILL_RETRY
                              ? " Retrying"
                              : " Connection closed")
                      << ". Error code " << code;
    }

    d_channels.resetAll();

    // Clear buffered frames in the framer
    d_framer = Framer();

    d_hungTimer->cancel();
    d_heartbeatManager->stop();

    if (d_socketConnection) {
        d_socketConnection.reset();
    }

    d_metricPublisher->publishCounter("disconnect_events", 1, d_vhostTags);

    d_state = Connection::DISCONNECTED;
    BALL_LOG_TRACE << "State now set to: " << d_state;
    if (dcType == WILL_RETRY) {
        d_retryHandler->retry(
            bdlf::BindUtil::bind(&Connection::retry, weak_from_this()));
    }
    else {
        if (d_firstConnectCb) {
            d_firstConnectCb(false); // Failure
            d_firstConnectCb = ConnectedCallback();
        }

        if (d_closeCb) {
            d_closeCb();
            d_closeCb = CloseFinishCallback();
        }
    }
}

class Connection::ConnectionMethodProcessor {
    Connection& conn;

  public:
    ConnectionMethodProcessor(Connection& conn)
    : conn(conn)
    {
    }

    void operator()(const rmqamqpt::ConnectionStart& start) const
    {
        if (expectedState(start, Connection::PROTOCOL_HEADER_SENT)) {
            conn.d_state = CONNECTION_START_RECEIVED;
            BALL_LOG_TRACE << "State now set to: " << conn.d_state;
            conn.sendConnectionStartOk();
        }
    }

    void operator()(const rmqamqpt::ConnectionTune& tune) const
    {
        if (expectedState(tune, Connection::CONNECTION_STARTOK_SENT)) {
            conn.d_state = CONNECTION_TUNE_RECEIVED;
            BALL_LOG_TRACE << "State now set to: " << conn.d_state;
            conn.sendConnectionTuneOk(tune);
            conn.sendConnectionOpen();
        }
    }

    void operator()(const rmqamqpt::ConnectionOpenOk& openok) const
    {
        if (expectedState(openok, Connection::CONNECTION_OPEN_SENT)) {
            BALL_LOG_INFO << "Connected " << conn.connectionDebugName();

            conn.d_state = CONNECTED;
            BALL_LOG_TRACE << "State now set to: " << conn.d_state;
            conn.d_hungTimer->cancel();
            conn.d_retryHandler->success();

            conn.d_metricPublisher->publishDistribution(
                conn.d_hasBeenConnected ? "reconnect_time" : "connect_time",
                (bdlt::CurrentTime::now() - conn.d_connectStartTime)
                    .totalSecondsAsDouble(),
                conn.d_vhostTags);

            conn.d_hasBeenConnected = true;

            conn.d_channels.openAll();

            if (conn.d_firstConnectCb) {
                const bool result = true;
                conn.d_firstConnectCb(result);
                conn.d_firstConnectCb = ConnectedCallback();
            }
        }
    }

    void operator()(const rmqamqpt::ConnectionClose& closeMethod) const
    {
        // Response of CLOSE method should always be CLOSE-OK
        // https://www.rabbitmq.com/resources/specs/amqp0-9-1.extended.xml
        BALL_LOG_INFO << "Received Connection.Close method from server. "
                      << closeMethod;
        conn.d_state = CONNECTION_CLOSE_RECEIVED;
        BALL_LOG_TRACE << "State now set to: " << conn.d_state;
        conn.sendConnectionCloseOk();

        if (closeMethod.classId() || closeMethod.methodId()) {
            conn.d_retryHandler->errorCallback()(
                "Connection=" + conn.d_connectionName + " Connection error " +
                    closeMethod.replyText(),
                closeMethod.replyCode());
        }
    }

    void operator()(const rmqamqpt::ConnectionCloseOk&) const
    {
        BALL_LOG_TRACE << "Received CLOSE-OK method from server";
        const DisconnectType dcType =
            conn.d_state == CONNECTION_CLOSE_SENT_EXIT ? FATAL : WILL_RETRY;
        conn.closeSocket(dcType);
    }

    template <typename T>
    void operator()(const T& method) const
    {
        BALL_LOG_ERROR << "Received invalid method: " << method
                       << ", Class type: "
                       << rmqamqpt::ConnectionMethod::CLASS_ID
                       << " Method type: " << method.METHOD_ID;

        conn.connectionException(rmqamqpt::Constants::NOT_IMPLEMENTED,
                                 "Method not implemented",
                                 rmqamqpt::ConnectionMethod::CLASS_ID,
                                 method.METHOD_ID);
    }

    void operator()(const BloombergLP::bslmf::Nil) const
    {
        BALL_LOG_ERROR << "This should never happen";
    }

    template <typename T>
    bool expectedState(const T& t, Connection::State expectedState) const
    {
        if (conn.d_state != expectedState) {
            BALL_LOG_ERROR << "received: " << t << ", whilst in "
                           << conn.d_state << " state, but expected state is "
                           << expectedState;
            conn.connectionException(rmqamqpt::Constants::UNEXPECTED_FRAME,
                                     "Unexpected frame",
                                     rmqamqpt::ConnectionMethod::CLASS_ID,
                                     T::METHOD_ID);
            return false;
        }
        return true;
    }
};

void Connection::processConnectionMethod(
    const rmqamqpt::ConnectionMethod& method)
{
    ConnectionMethodProcessor cmp(*this);
    method.apply(cmp);
}

void Connection::processMethod(const rmqamqpt::Method& method)
{
    switch (method.classId()) {
        case rmqamqpt::Constants::CONNECTION: {
            processConnectionMethod(method.the<rmqamqpt::ConnectionMethod>());
            break;
        }
        default: {
            connectionException(rmqamqpt::Constants::NOT_IMPLEMENTED,
                                "Not implemented Class Processor",
                                method.classId());
        }
    }
}

void Connection::readHandler(const bsl::weak_ptr<Connection>& weakConn,
                             const rmqamqpt::Frame& frame)
{
    bsl::shared_ptr<Connection> conn = weakConn.lock();

    if (!conn) {
        BALL_LOG_DEBUG
            << "Read data after destruction - socket close in progress";
        return;
    }

    conn->processNextFrame(frame);
}

void Connection::processNextFrame(const rmqamqpt::Frame& frame)
{
    d_heartbeatManager->notifyMessageReceived();

    uint16_t channel = 0;
    Message received;
    const Framer::ReturnCode rc =
        d_framer.appendFrame(&channel, &received, frame);

    if (rc == Framer::PARTIAL) {
        // Waiting for other related frames like content body frames
        return;
    }

    if (rc != Framer::OK) {
        connectionException(rmqamqpt::Constants::SYNTAX_ERROR,
                            "Cannot decode payload");
        return;
    }

    BALL_LOG_TRACE << "Received message: MESSAGE=" << received
                   << " CHANNEL=" << frame.channel()
                   << " LEN=" << frame.payloadLength();

    if (channel == 0) {
        // Handle control channel messages
        if (received.is<rmqamqpt::Method>()) {
            processMethod(received.the<rmqamqpt::Method>());
        }
        else if (received.is<rmqamqpt::Heartbeat>()) {
            // Already notified receipt above
            BALL_LOG_DEBUG << "Received Heartbeat from server";
            d_heartbeatManager->notifyHeartbeatReceived();
        }
        else {
            connectionException(rmqamqpt::Constants::NOT_IMPLEMENTED,
                                "Not implemented non-method frame");
        }
    }
    else {
        // Handle channel messages
        bool channelExists = d_channels.processReceived(channel, received);

        if (!channelExists) {
            connectionException(rmqamqpt::Constants::CHANNEL_ERROR,
                                "Received Message on unknown channel: " +
                                    bsl::to_string(channel));
        }
    }
}

void Connection::startHeartbeatManager(uint32_t timeout)
{
    using bdlf::PlaceHolders::_1;
    d_heartbeatManager->start(
        timeout,
        bdlf::BindUtil::bind(&Connection::sendHeartbeat, this, _1),
        bdlf::BindUtil::bind(&Connection::killConnection, this));
}

void Connection::sendHeartbeat(const rmqamqpt::Frame& heartbeat)
{
    asyncWriteSingleFrame(serializeFrame(heartbeat), &noopWriteComplete);
}

void Connection::killConnection()
{
    BALL_LOG_FATAL << "Missing heartbeats detected - Disconnecting "
                   << connectionDebugName();

    d_metricPublisher->publishCounter("heartbeat_timeouts", 1, d_vhostTags);
    closeSocket(WILL_RETRY);
}

void Connection::asyncWriteSingleFrame(
    const bsl::shared_ptr<rmqio::SerializedFrame>& serializedFrame,
    const rmqio::Connection::SuccessWriteCallback& callback)
{

    if (!d_socketConnection) {
        return;
    }

    d_socketConnection->asyncWrite(
        bsl::vector<bsl::shared_ptr<rmqio::SerializedFrame> >(1,
                                                              serializedFrame),
        callback);
    d_heartbeatManager->notifyMessageSent();
}

void Connection::handleAsyncChannelSendWeakPtr(
    const bsl::weak_ptr<Connection>& weakSelf,
    uint16_t channel,
    const bsl::shared_ptr<rmqamqp::Message>& message,
    const rmqio::Connection::SuccessWriteCallback& callback)
{
    bsl::shared_ptr<Connection> self = weakSelf.lock();

    if (!self) {
        BALL_LOG_DEBUG << "Channel attempted to send message after its "
                          "connection has destructed.";
        return;
    }

    self->handleAsyncChannelSend(channel, message, callback);
}

void Connection::handleAsyncChannelSend(
    uint16_t channel,
    const bsl::shared_ptr<rmqamqp::Message>& message,
    const rmqio::Connection::SuccessWriteCallback& callback)
{
    if (!d_socketConnection) {
        return;
    }

    BALL_LOG_TRACE << "Sending Method: Message=" << *message
                   << " CHANNEL=" << channel;

    bsl::vector<rmqamqpt::Frame> frames;
    d_framer.makeFrames(&frames, channel, *message);

    if (frames.size() == 0) {
        BALL_LOG_ERROR
            << "Attempted to send a message which doesn't serialize: "
            << message;
        return;
    }

    using bdlf::PlaceHolders::_1;

    bsl::vector<bsl::shared_ptr<rmqio::SerializedFrame> > serializedFrames;
    serializedFrames.reserve(frames.size());
    bsl::transform(frames.cbegin(),
                   frames.cend(),
                   bsl::back_inserter(serializedFrames),
                   &serializeFrame);

    d_socketConnection->asyncWrite(serializedFrames, callback);
    d_heartbeatManager->notifyMessageSent();
}

rmqt::Future<ReceiveChannel> Connection::createTopologySyncedReceiveChannel(
    const rmqt::Topology& topology,
    const rmqt::ConsumerConfig& config,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
    const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue)
{
    bsl::shared_ptr<ReceiveChannel> rc =
        createReceiveChannel(topology, retryHandler, config, ackQueue);
    return rc->waitForReady().then<rmqamqp::ReceiveChannel>(passOnSuccess(rc));
}

rmqt::Future<SendChannel> Connection::createTopologySyncedSendChannel(
    const rmqt::Topology& topology,
    const bsl::shared_ptr<rmqt::Exchange>& exchange,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler)
{
    bsl::shared_ptr<SendChannel> sc =
        createSendChannel(topology, exchange, retryHandler);
    return sc->waitForReady().then<SendChannel>(passOnSuccess(sc));
}

bsl::shared_ptr<ReceiveChannel> Connection::createReceiveChannel(
    const rmqt::Topology& topology,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
    const rmqt::ConsumerConfig& config,
    const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue)
{
    const uint16_t channelId = d_channels.assignId();

    using namespace bdlf::PlaceHolders;

    bsl::shared_ptr<ReceiveChannel> receiveChannel =
        d_channelFactory->createReceiveChannel(
            topology,
            bdlf::BindUtil::bind(&Connection::handleAsyncChannelSendWeakPtr,
                                 weak_from_this(),
                                 channelId,
                                 _1,
                                 _2),
            retryHandler,
            d_metricPublisher,
            config,
            d_endpoint->vhost(),
            ackQueue,
            d_timerFactory->createWithTimeout(
                bsls::TimeInterval(Channel::k_HUNG_CHANNEL_TIMER_SEC)),
            bdlf::BindUtil::bind(&Connection::channelHung, weak_from_this()));

    d_channels.associateChannel(channelId, receiveChannel);

    if (d_state == CONNECTED) {
        receiveChannel->open();
    }

    return receiveChannel;
}

bsl::shared_ptr<SendChannel> Connection::createSendChannel(
    const rmqt::Topology& topology,
    const bsl::shared_ptr<rmqt::Exchange>& exchange,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler)
{
    const uint16_t channelId = d_channels.assignId();

    using namespace bdlf::PlaceHolders;

    bsl::shared_ptr<SendChannel> sendChannel =
        d_channelFactory->createSendChannel(
            topology,
            exchange,
            bdlf::BindUtil::bind(&Connection::handleAsyncChannelSendWeakPtr,
                                 weak_from_this(),
                                 channelId,
                                 _1,
                                 _2),
            retryHandler,
            d_metricPublisher,
            d_endpoint->vhost(),
            d_timerFactory->createWithTimeout(
                bsls::TimeInterval(Channel::k_HUNG_CHANNEL_TIMER_SEC)),
            bdlf::BindUtil::bind(&Connection::channelHung, weak_from_this()));

    d_channels.associateChannel(channelId, sendChannel);

    if (d_state == CONNECTED) {
        sendChannel->open();
    }

    return sendChannel;
}

void Connection::connectionHung(rmqio::Timer::InterruptReason reason)
{
    if (reason == rmqio::Timer::EXPIRE) {
        d_metricPublisher->publishCounter(
            "hung_connection_reset", 1, d_vhostTags);

        BALL_LOG_FATAL << "Timed out after " << k_HUNG_TIMER_SEC
                       << " seconds while connecting: "
                       << connectionDebugName();
        closeSocket(WILL_RETRY);
    }
}

void Connection::channelHung(const bsl::weak_ptr<Connection>& self)
{
    bsl::shared_ptr<Connection> lockedSelf = self.lock();

    if (lockedSelf) {
        lockedSelf->d_metricPublisher->publishCounter(
            "hung_connection_reset", 1, lockedSelf->d_vhostTags);
        lockedSelf->closeSocket(WILL_RETRY);
    }
    // else we're shutting down
}

bsl::string Connection::connectionDebugName() const
{
    return d_connectionName + ": " + d_endpoint->formatAddress();
}

Connection::Factory::Factory(
    const bsl::shared_ptr<rmqio::Resolver>& resolver,
    const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory,
    const rmqt::ErrorCallback& errorCb,
    const rmqt::SuccessCallback& successCb,
    const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
    const bsl::shared_ptr<ConnectionMonitor>& connectionMonitor,
    const rmqt::FieldTable& clientProperties,
    const bsl::optional<bsls::TimeInterval>& connectionErrorThreshold)
: d_errorCb(errorCb)
, d_successCb(successCb)
, d_clientProperties(clientProperties)
, d_metricPublisher(metricPublisher)
, d_resolver(resolver)
, d_timerFactory(timerFactory)
, d_connectionMonitor(connectionMonitor)
, d_connectionErrorThreshold(connectionErrorThreshold)
{
}

bsl::shared_ptr<Connection> Connection::Factory::create(
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqt::Credentials>& credentials,
    const bsl::string& name)
{
    bsl::shared_ptr<rmqamqp::Connection> result(new rmqamqp::Connection(
        d_resolver,
        newRetryHandler(),
        newHeartBeatManager(),
        d_timerFactory,
        newChannelFactory(),
        d_metricPublisher,
        endpoint,
        credentials,
        generateClientProperties(d_clientProperties, name),
        name));

    d_connectionMonitor->addConnection(bsl::weak_ptr<Connection>(result));

    return result;
}

bsl::shared_ptr<rmqio::RetryHandler> Connection::Factory::newRetryHandler()
{
    return d_connectionErrorThreshold
               ? bsl::shared_ptr<rmqio::RetryHandler>(
                     bsl::make_shared<rmqio::ConnectionRetryHandler>(
                         d_timerFactory,
                         d_errorCb,
                         d_successCb,
                         bsl::make_shared<rmqio::BackoffLevelRetryStrategy>(),
                         *d_connectionErrorThreshold))
               : bsl::make_shared<rmqio::RetryHandler>(
                     d_timerFactory,
                     d_errorCb,
                     d_successCb,
                     bsl::make_shared<rmqio::BackoffLevelRetryStrategy>());
}

bsl::shared_ptr<rmqamqp::HeartbeatManager>
Connection::Factory::newHeartBeatManager()
{
    return bsl::make_shared<rmqamqp::HeartbeatManagerImpl>(d_timerFactory);
}

bsl::shared_ptr<rmqamqp::ChannelFactory>
Connection::Factory::newChannelFactory()
{
    return bsl::make_shared<rmqamqp::ChannelFactory>();
}

} // namespace rmqamqp
} // namespace BloombergLP
