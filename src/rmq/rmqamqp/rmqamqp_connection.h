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

#ifndef INCLUDED_RMQAMQP_CONNECTION
#define INCLUDED_RMQAMQP_CONNECTION

#include <rmqamqp_channel.h>
#include <rmqamqp_channelcontainer.h>
#include <rmqamqp_channelfactory.h>
#include <rmqamqp_channelmap.h>
#include <rmqamqp_connectionmonitor.h>
#include <rmqamqp_framer.h>
#include <rmqamqp_heartbeatmanager.h>

#include <rmqio_eventloop.h>
#include <rmqio_resolver.h>
#include <rmqio_retryhandler.h>
#include <rmqio_timer.h>
#include <rmqp_metricpublisher.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_credentials.h>
#include <rmqt_endpoint.h>
#include <rmqt_message.h>

#include <ball_log.h>
#include <bslmt_mutex.h>
#include <bsls_timeinterval.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_optional.h>
#include <bsl_string.h>
#include <bsl_string_view.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {
class ConnectionMethod;
class ConnectionTune;
class Method;
} // namespace rmqamqpt
namespace rmqio {
class Resolver;
class Connection;
} // namespace rmqio
namespace rmqamqp {

class Connection : public bsl::enable_shared_from_this<Connection>,
                   public ChannelContainer {
  public:
    class Factory;

    enum DisconnectType { FATAL, WILL_RETRY };

    typedef bsl::function<void(const rmqt::Message&)> MessageCallback;
    typedef bsl::function<void(bool /* success */)> ConnectedCallback;
    typedef bsl::function<void(void)> CloseFinishCallback;

    enum State {
        DISCONNECTED,
        PROTOCOL_HEADER_SENT,
        CONNECTION_START_RECEIVED,
        CONNECTION_STARTOK_SENT,
        CONNECTION_TUNE_RECEIVED,
        CONNECTION_TUNEOK_SENT,
        CONNECTION_OPEN_SENT,
        CONNECTION_OPENOK_RECEIVED,
        CONNECTION_CLOSE_SENT_EXIT,
        CONNECTION_CLOSE_SENT_RECONNECT,
        CONNECTION_CLOSE_RECEIVED,
        CONNECTION_CLOSEOK_SENT,
        CONNECTION_CLOSED,
        CONNECTED
    };

    /// During destruction the connection is cleanly closed. After the
    /// destructor completes no callbacks will be called.
    virtual ~Connection();

    /// Declares a new channel which can be used to receive messages
    /// The function must be called from the Connection's EventLoop thread.
    /// Calling this function when disconnected will successfully return a
    /// channel object. The future resolves when the connection and channel is
    /// established and the topology is sync'd.
    virtual rmqt::Future<ReceiveChannel> createTopologySyncedReceiveChannel(
        const rmqt::Topology& topology,
        const rmqt::ConsumerConfig& config,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue);

    /// Declares a new channel which can be used to publish messages
    /// The function must be called from the Connection's EventLoop thread.
    /// Calling this function when disconnected will successfully return a
    /// send channel future object.  The future resolves when the connection and
    /// channel is established and the topology is sync'd.
    virtual rmqt::Future<SendChannel> createTopologySyncedSendChannel(
        const rmqt::Topology& topology,
        const bsl::shared_ptr<rmqt::Exchange>& exchange,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler);

    /// Declares a new channel which can be used to receive messages
    /// The function must be called from the Connection's EventLoop thread.
    /// Calling this function when disconnected will successfully return a
    /// receive channel future object. The channel will be opened when the
    /// connection is established
    bsl::shared_ptr<ReceiveChannel> createReceiveChannel(
        const rmqt::Topology& topology,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const rmqt::ConsumerConfig& config,
        const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue);

    /// Declares a new channel which can be used to publish messages
    /// The function must be called from the Connection's EventLoop thread.
    /// Calling this function when disconnected will successfully return a
    /// channel object. The channel will be opened when the connection is
    /// established
    bsl::shared_ptr<SendChannel>
    createSendChannel(const rmqt::Topology& topology,
                      const bsl::shared_ptr<rmqt::Exchange>& exchange,
                      const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler);

    State state() const { return d_state; }

    /// Initiates a graceful connection close. closeCallback is invoked once the
    /// connection has been closed.
    /// This method is virtual for testing purposes.
    virtual void close(const CloseFinishCallback& closeCallback);

    void startFirstConnection(const ConnectedCallback& connectedCallback);

    const ChannelMap& channelMap() const BSLS_KEYWORD_OVERRIDE
    {
        return d_channels;
    }

    bsl::string
    connectionDebugName() const BSLS_KEYWORD_OVERRIDE BSLS_KEYWORD_FINAL;

  protected:
    /// Constructs + begins connecting to the given AMQP endpoint
    /// \param resolver        Used to create sockets to the broker
    /// \param retryhandler    Used to retry connection
    /// \param hbManager       Used to manage heartbeat mechanism
    /// \param timerFactory    Factory providing timers
    /// \param channelFactory  Factory that setups up the channels
    /// \param metricPublisher Publish connection-related metrics
    ///                        connect/channel events
    /// \param endpoint        references the vhost this connection is
    ///                        joining
    /// \param credentials     Credentials used to connect to the broker
    Connection(const bsl::shared_ptr<rmqio::Resolver>& resolver,
               const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
               const bsl::shared_ptr<rmqamqp::HeartbeatManager>& hbManager,
               const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory,
               const bsl::shared_ptr<rmqamqp::ChannelFactory>& channelFactory,
               const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
               const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
               const bsl::shared_ptr<rmqt::Credentials>& credentials,
               const rmqt::FieldTable& clientProperties,
               bsl::string_view name);

  private:
    bsl::shared_ptr<rmqio::Resolver> d_resolver;
    bsl::shared_ptr<rmqio::RetryHandler> d_retryHandler;
    bsl::shared_ptr<rmqamqp::HeartbeatManager> d_heartbeatManager;
    bsl::shared_ptr<rmqt::Endpoint> d_endpoint;
    bsl::shared_ptr<rmqt::Credentials> d_credentials;

    class ConnectionMethodProcessor;

    bsl::shared_ptr<rmqio::Connection> d_socketConnection;
    bsl::shared_ptr<rmqamqp::ChannelFactory> d_channelFactory;
    bsl::shared_ptr<rmqp::MetricPublisher> d_metricPublisher;
    Framer d_framer;
    State d_state;
    rmqt::FieldTable d_clientProperties;
    ChannelMap d_channels;
    bsl::shared_ptr<rmqio::Timer> d_hungTimer;

    bsl::shared_ptr<rmqio::TimerFactory> d_timerFactory;

    ConnectedCallback d_firstConnectCb;
    CloseFinishCallback d_closeCb;

    bsls::TimeInterval d_connectStartTime;
    bool d_hasBeenConnected; // For metrics purposes -- indicates whether this
                             // instance has successfully connected at least
                             // once so far

    bsl::vector<bsl::pair<bsl::string, bsl::string> > d_vhostTags;
    bsl::string d_connectionName;

    /// Initiate connection
    void initiateConnect();

    // ConnectionMethods
    void sendConnectionStartOk();
    void sendConnectionTuneOk(const rmqamqpt::ConnectionTune& tuneMethod);
    void sendConnectionOpen();
    void sendConnectionCloseOk();

    void sendConnectionClose(rmqamqpt::Constants::AMQPReplyCode replyCode,
                             const bsl::string& replyText,
                             rmqamqpt::Constants::AMQPClassId failingClassId =
                                 rmqamqpt::Constants::NO_CLASS,
                             rmqamqpt::Constants::AMQPMethodId failingMethodId =
                                 rmqamqpt::Constants::NO_METHOD);

    void connectionException(rmqamqpt::Constants::AMQPReplyCode replyCode,
                             const bsl::string& replyText,
                             rmqamqpt::Constants::AMQPClassId failingClassId =
                                 rmqamqpt::Constants::NO_CLASS,
                             rmqamqpt::Constants::AMQPMethodId failingMethodId =
                                 rmqamqpt::Constants::NO_METHOD);

    // Callback for writes
    void onWriteComplete(State newState);

    // Callback for writes
    static void onWriteCompleteCb(bsl::weak_ptr<Connection> weakSelf,
                                  State newState);

    // Connection Method Router
    void processConnectionMethod(const rmqamqpt::ConnectionMethod& method);

    // Frame Method Router
    void processMethod(const rmqamqpt::Method& method);

    static void readHandler(const bsl::weak_ptr<Connection>& weakConn,
                            const rmqamqpt::Frame& frame);

    // Callback for new frames
    void processNextFrame(const rmqamqpt::Frame& frame);

    /// Async callback
    void connect();

    /// Async callback
    void connectError(const rmqio::Resolver::Error& error);

    /// Async callback
    static void connectCb(const bsl::weak_ptr<Connection> weakSelf);

    /// Async callback
    static void connectErrorCb(const bsl::weak_ptr<Connection> weakSelf,
                               const rmqio::Resolver::Error& error);

    static void socketError(const bsl::weak_ptr<Connection>& weakConn,
                            const rmqio::Connection::ReturnCode& rc);

    // Async, registers callback for read Messages
    void readMessages(MessageCallback cb);

    // Async, called back when disconnection occurs
    void onDisconnect();

    /// Ask rmqio::Connection to close. Calls socketShutdown when complete
    void closeSocket(DisconnectType disconnectType);

    /// Destruct rmqio::Connection and optionally begin retrying
    void socketShutdown(DisconnectType disconnectType,
                        rmqio::Connection::ReturnCode rc);

    /// Callback for retrying connection
    static void retry(const bsl::weak_ptr<Connection>& weakSelf);

    void startHeartbeatManager(uint32_t timeout);

    void sendHeartbeat(const rmqamqpt::Frame&);

    void killConnection();

    void connectionHung(rmqio::Timer::InterruptReason reason);

    static void channelHung(const bsl::weak_ptr<Connection>& self);

    static void handleAsyncChannelSendWeakPtr(
        const bsl::weak_ptr<Connection>& weakSelf,
        uint16_t channel,
        const bsl::shared_ptr<rmqamqp::Message>& message,
        const rmqio::Connection::SuccessWriteCallback& callback);

    void handleAsyncChannelSend(
        uint16_t channel,
        const bsl::shared_ptr<rmqamqp::Message>& message,
        const rmqio::Connection::SuccessWriteCallback& callback);

    void asyncWriteSingleFrame(
        const bsl::shared_ptr<rmqio::SerializedFrame>& frame,
        const rmqio::Connection::SuccessWriteCallback& callback);
};

class Connection::Factory {
  public:
    // CREATORS

    Factory(const bsl::shared_ptr<rmqio::Resolver>& resolver,
            const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory,
            const rmqt::ErrorCallback& errorCb,
            const rmqt::SuccessCallback& successCb,
            const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
            const bsl::shared_ptr<ConnectionMonitor>& connectionMonitor,
            const rmqt::FieldTable& clientProperties,
            const bsl::optional<bsls::TimeInterval>& connectionErrorThreshold);

    virtual ~Factory() {}

    virtual bsl::shared_ptr<Connection>
    create(const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
           const bsl::shared_ptr<rmqt::Credentials>& credentials,
           const bsl::string& name = "");

  protected:
    virtual bsl::shared_ptr<rmqio::RetryHandler> newRetryHandler();
    virtual bsl::shared_ptr<rmqamqp::HeartbeatManager> newHeartBeatManager();
    virtual bsl::shared_ptr<rmqamqp::ChannelFactory> newChannelFactory();

  private:
    Factory(const Factory&) BSLS_KEYWORD_DELETED;
    Factory& operator=(const Factory&) BSLS_KEYWORD_DELETED;

    const rmqt::ErrorCallback d_errorCb;
    const rmqt::SuccessCallback d_successCb;
    const rmqt::FieldTable d_clientProperties;
    const bsl::shared_ptr<rmqp::MetricPublisher> d_metricPublisher;
    const bsl::shared_ptr<rmqio::Resolver> d_resolver;
    const bsl::shared_ptr<rmqio::TimerFactory> d_timerFactory;
    const bsl::shared_ptr<ConnectionMonitor> d_connectionMonitor;
    const bsl::optional<bsls::TimeInterval> d_connectionErrorThreshold;
}; // class Connection::Factory
} // namespace rmqamqp
} // namespace BloombergLP

#endif
