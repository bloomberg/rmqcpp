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

#include <rmqtestutil_mockchannel.t.h>
#include <rmqtestutil_mockmetricpublisher.h>
#include <rmqtestutil_mockresolver.t.h>
#include <rmqtestutil_mockretryhandler.t.h>
#include <rmqtestutil_mocktimerfactory.h>
#include <rmqtestutil_replayframe.h>

#include <rmqamqp_channelcontainer.h>
#include <rmqamqp_connectionmonitor.h>
#include <rmqamqp_metrics.h>
#include <rmqamqpt_frame.h>
#include <rmqio_connection.h>
#include <rmqio_retryhandler.h>
#include <rmqio_serializedframe.h>
#include <rmqp_metricpublisher.h>
#include <rmqt_consumerack.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_fieldvalue.h>
#include <rmqt_plaincredentials.h>
#include <rmqt_simpleendpoint.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bsl_functional.h>
#include <bsl_iostream.h>
#include <bsl_limits.h>
#include <bsl_memory.h>
#include <bsl_stdexcept.h>
#include <bsl_utility.h>
#include <bsl_vector.h>
#include <bsls_assert.h>
#include <bsls_keyword.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqp;
using namespace rmqio;
using namespace rmqtestutil;
using namespace rmqamqpt;
using namespace ::testing;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.CONNECTION.TESTS")

const char* TEST_VHOST = "vhostname";

void noOpOnError(const bsl::string&, int) {}

void noopCloseHandler() {}

void gracefulCloseHandler(bool& invoked) { invoked = true; }

rmqt::FieldTable generateDefaultClientProperties(
    const bsl::string& connectionName = "test-connection")
{
    rmqt::FieldTable props;
    bsl::shared_ptr<rmqt::FieldTable> capabilities =
        bsl::make_shared<rmqt::FieldTable>();

    (*capabilities)["connection.blocked"]           = rmqt::FieldValue(false);
    (*capabilities)["authentication_failure_close"] = rmqt::FieldValue(true);
    (*capabilities)["consumer_cancel_notify"]       = rmqt::FieldValue(true);
    (*capabilities)["publisher_confirms"]           = rmqt::FieldValue(true);
    (*capabilities)["basic.nack"]                   = rmqt::FieldValue(false);

    props["capabilities"] = rmqt::FieldValue(capabilities);

    props["platform"] =
        rmqt::FieldValue(bsl::string(rmqamqpt::Constants::PLATFORM));
    props["product"] =
        rmqt::FieldValue(bsl::string(rmqamqpt::Constants::PRODUCT));
    props["version"] =
        rmqt::FieldValue(bsl::string(rmqamqpt::Constants::VERSION));

    if (!connectionName.empty()) {
        props["connection_name"] = rmqt::FieldValue(connectionName);
    }

    return props;
}

class MockConnection : public rmqio::Connection {
  public:
    MockConnection(const rmqio::Connection::Callbacks& callbacks,
                   ReplayFrame& replayFrame,
                   boost::asio::io_context& eventLoop)
    : d_replayFrame(replayFrame)
    , d_connectionCallbacks(callbacks)
    , d_eventLoop(eventLoop)
    {
    }

    rmqamqpt::Method examineFrameMethod(const SerializedFrame& frame)
    {
        rmqamqpt::Frame decoded;
        size_t unused1 = 0, unused2 = 0;
        const Frame::ReturnCode rc = Frame::decode(&decoded,
                                                   &unused1,
                                                   &unused2,
                                                   frame.serialized(),
                                                   frame.frameLength());
        if (rc != Frame::OK) {
            return rmqamqpt::Method();
        }
        rmqamqp::Framer framer;
        uint16_t channel;
        rmqamqp::Message msg;
        if (framer.appendFrame(&channel, &msg, decoded) ==
                rmqamqp::Framer::OK &&
            msg.is<rmqamqpt::Method>()) {
            return msg.the<rmqamqpt::Method>();
        }
        return rmqamqpt::Method();
    }

    void feedNextFrame()
    {
        if (d_replayFrame.isNextDirection(ReplayFrame::INBOUND)) {
            rmqamqpt::Frame decoded;
            bsl::shared_ptr<SerializedFrame> toSend = d_replayFrame.getFrame();

            size_t unused1 = 0, unused2 = 0;
            const Frame::ReturnCode rc = Frame::decode(&decoded,
                                                       &unused1,
                                                       &unused2,
                                                       toSend->serialized(),
                                                       toSend->frameLength());

            BSLS_ASSERT_OPT(rc == Frame::OK);

            boost::asio::post(
                d_eventLoop,
                bdlf::BindUtil::bind(d_connectionCallbacks.onRead, decoded));
        }
    }

    void close(const DoneCallback& cb)
    {
        BALL_LOG_TRACE << "MockConnection close";

        boost::asio::post(d_eventLoop,
                          bdlf::BindUtil::bind(cb, GRACEFUL_DISCONNECT));
    }

    void asyncWriteImpl(
        const bsl::vector<bsl::shared_ptr<rmqio::SerializedFrame> >& frames,
        const rmqio::Connection::SuccessWriteCallback& callback)
    {
        for (bsl::vector<
                 bsl::shared_ptr<rmqio::SerializedFrame> >::const_iterator it =
                 frames.cbegin();
             it < frames.cend();
             ++it) {
            BSLS_ASSERT_OPT(*(d_replayFrame.writeFrame()) == **it);
        }
        bool closeOk = rmqamqpt::Method::Util::typeMatch(
            examineFrameMethod(*frames[0]),
            rmqamqpt::Method(
                rmqamqpt::ConnectionMethod(rmqamqpt::ConnectionCloseOk())));

        boost::asio::post(d_eventLoop, callback);

        if (!closeOk) {
            feedNextFrame();
        }
        else {
            BALL_LOG_INFO << "Not replying to closeok";
        }
    }

    MOCK_CONST_METHOD0(isConnected, bool());
    MOCK_METHOD2(
        asyncWrite,
        void(const bsl::vector<bsl::shared_ptr<rmqio::SerializedFrame> >&,
             const rmqio::Connection::SuccessWriteCallback&));

    ReplayFrame& d_replayFrame;
    rmqio::Connection::Callbacks d_connectionCallbacks;
    boost::asio::io_context& d_eventLoop;
};

class MockHeartbeatManager : public rmqamqp::HeartbeatManager {
  public:
    MockHeartbeatManager() {}

    void start(uint32_t,
               const HeartbeatCallback& sendHeartbeat,
               const ConnectionDeathCallback& onConnectionDeath)
    {
        d_sendHeartbeat  = sendHeartbeat;
        d_killConnection = onConnectionDeath;
    }

    MOCK_METHOD0(stop, void());
    MOCK_METHOD0(notifyMessageSent, void());
    MOCK_METHOD0(notifyMessageReceived, void());
    MOCK_METHOD0(notifyHeartbeatReceived, void());

    rmqamqp::HeartbeatManager::HeartbeatCallback d_sendHeartbeat;
    rmqamqp::HeartbeatManager::ConnectionDeathCallback d_killConnection;
};

class Callbacks {
  public:
    virtual void onMessage(const rmqt::Message&)            = 0;
    virtual void onWriteCompleteCb(rmqamqp::Channel::State) = 0;

    virtual void onConnect(bool success) = 0;
};

class MockCallbacks : public Callbacks {
  public:
    MOCK_METHOD1(onMessage, void(const rmqt::Message&));
    MOCK_METHOD1(onWriteCompleteCb, void(rmqamqp::Channel::State));

    MOCK_METHOD1(onConnect, void(bool));
};

class MockChannelFactory : public rmqamqp::ChannelFactory {
  public:
    MockChannelFactory()
    : d_mockReceiveChannelCreated(false)
    , d_mockSendChannelCreated(false)
    {
    }

    MOCK_METHOD9(createReceiveChannel,
                 bsl::shared_ptr<ReceiveChannel>(
                     const rmqt::Topology&,
                     const Channel::AsyncWriteCallback&,
                     const bsl::shared_ptr<rmqio::RetryHandler>&,
                     const bsl::shared_ptr<rmqp::MetricPublisher>&,
                     const rmqt::ConsumerConfig&,
                     const bsl::string&,
                     const bsl::shared_ptr<rmqt::ConsumerAckQueue>&,
                     const bsl::shared_ptr<rmqio::Timer>&,
                     const Channel::HungChannelCallback&));

    MOCK_METHOD8(createSendChannel,
                 bsl::shared_ptr<SendChannel>(
                     const rmqt::Topology&,
                     const bsl::shared_ptr<rmqt::Exchange>&,
                     const Channel::AsyncWriteCallback&,
                     const bsl::shared_ptr<rmqio::RetryHandler>&,
                     const bsl::shared_ptr<rmqp::MetricPublisher>&,
                     const bsl::string&,
                     const bsl::shared_ptr<rmqio::Timer>&,
                     const Channel::HungChannelCallback&));

    bool d_mockReceiveChannelCreated;
    bool d_mockSendChannelCreated;
};

ACTION_P(OnError, mockConnectptr)
{
    (mockConnectptr.get())
        ->d_connectionCallbacks.onError(rmqio::Connection::DISCONNECTED_ERROR);
}

ACTION_P(ReceiveChannelCreated, channelFactory)
{
    (channelFactory.get())->d_mockReceiveChannelCreated = true;
}

ACTION_P(SendChannelCreated, channelFactory)
{
    (channelFactory.get())->d_mockSendChannelCreated = true;
}

ACTION_P(DefaultAsyncWrite, mockConnectptr)
{
    // This is an action just so asyncWrite can be mocked with an error as well
    // as support this default behaviour
    mockConnectptr.get()->asyncWriteImpl(arg0, arg1);
}

ACTION_P3(ConnectMockConnection, mockConnectPtrPtr, replayFrame, eventLoop)
{
    *mockConnectPtrPtr = bsl::make_shared<MockConnection>(
        arg3, bsl::ref(replayFrame), bsl::ref(eventLoop));

    EXPECT_CALL(**mockConnectPtrPtr, asyncWrite(_, _))
        .WillRepeatedly(DefaultAsyncWrite(bsl::ref(*mockConnectPtrPtr)));

    ON_CALL(**mockConnectPtrPtr, isConnected()).WillByDefault(Return(true));

    boost::asio::post(eventLoop.get(), arg4);

    return *mockConnectPtrPtr;
}

using namespace bdlf::PlaceHolders;

class MockConnectionMonitor : public ConnectionMonitor {
  public:
    void addConnection(const bsl::weak_ptr<ChannelContainer>&) {}
};

class ConnectionFactory : public rmqamqp::Connection::Factory {
    const bsl::shared_ptr<rmqio::RetryHandler> d_retryHandler;
    const bsl::shared_ptr<rmqamqp::HeartbeatManager> d_hbManager;
    const bsl::shared_ptr<rmqamqp::ChannelFactory> d_channelFactory;

  public:
    ConnectionFactory(
        const bsl::shared_ptr<rmqio::Resolver>& resolver,
        const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory,
        const rmqt::ErrorCallback& errorCb,
        const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
        const rmqt::FieldTable& clientProperties,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const bsl::shared_ptr<rmqamqp::HeartbeatManager>& hbManager,
        const bsl::shared_ptr<rmqamqp::ChannelFactory>& channelFactory)
    : rmqamqp::Connection::Factory(resolver,
                                   timerFactory,
                                   errorCb,
                                   metricPublisher,
                                   bsl::make_shared<MockConnectionMonitor>(),
                                   clientProperties,
                                   bsls::TimeInterval())
    , d_retryHandler(retryHandler)
    , d_hbManager(hbManager)
    , d_channelFactory(channelFactory)
    {
    }
    bsl::shared_ptr<rmqio::RetryHandler> newRetryHandler() BSLS_KEYWORD_OVERRIDE
    {
        return d_retryHandler;
    }

    bsl::shared_ptr<rmqamqp::HeartbeatManager>
    newHeartBeatManager() BSLS_KEYWORD_OVERRIDE
    {
        return d_hbManager;
    }

    bsl::shared_ptr<rmqamqp::ChannelFactory>
    newChannelFactory() BSLS_KEYWORD_OVERRIDE
    {
        return d_channelFactory;
    }
};
class ConnectionTests : public ::testing::Test {
  public:
    ReplayFrame d_replayFrame;
    rmqt::ErrorCallback d_errorCallback;
    bsl::shared_ptr<rmqtestutil::MockResolver> d_resolver;
    bsl::shared_ptr<rmqtestutil::MockRetryHandler> d_retryHandler;
    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
    bsl::shared_ptr<MockHeartbeatManager> d_heartbeat;
    bsl::shared_ptr<rmqtestutil::MockRetryHandler> d_retryHandlerChannel;
    bsl::shared_ptr<MockChannelFactory> d_channelFactory;
    bsl::shared_ptr<MockReceiveChannel> d_receiveChannel;
    bsl::shared_ptr<MockSendChannel> d_sendChannel;
    bsl::shared_ptr<rmqt::ConsumerAckQueue> d_ackQueue;
    bsl::shared_ptr<rmqtestutil::MockMetricPublisher> d_metricPublisher;
    rmqt::FieldTable d_clientProperties;

    bsl::shared_ptr<rmqamqp::Connection::Factory> d_factory;

    MockCallbacks d_callbacks;
    StrictMock<rmqio::Connection::SuccessWriteCallback> d_onWriteCompleteCb;
    StrictMock<rmqamqp::Connection::ConnectedCallback> d_onConnectCb;

    bsl::shared_ptr<rmqt::SimpleEndpoint> d_endpoint;
    bsl::shared_ptr<rmqt::PlainCredentials> d_credentials;

    bsl::shared_ptr<MockConnection> d_connection;

    bsl::vector<bsl::pair<bsl::string, bsl::string> > d_vhostTag;

    boost::asio::io_context d_eventLoop;

    ConnectionTests()
    : d_replayFrame()
    , d_errorCallback(noOpOnError)
    , d_resolver(bsl::make_shared<rmqtestutil::MockResolver>())
    , d_retryHandler(bsl::make_shared<rmqtestutil::MockRetryHandler>())
    , d_timerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    , d_heartbeat(bsl::make_shared<MockHeartbeatManager>())
    , d_retryHandlerChannel(bsl::make_shared<rmqtestutil::MockRetryHandler>())
    , d_channelFactory(bsl::make_shared<MockChannelFactory>())
    , d_receiveChannel(bsl::make_shared<MockReceiveChannel>(
          bsl::make_shared<rmqt::ConsumerAckQueue>(),
          d_retryHandlerChannel))
    , d_sendChannel(bsl::make_shared<MockSendChannel>(d_retryHandlerChannel))
    , d_ackQueue(bsl::make_shared<rmqt::ConsumerAckQueue>())
    , d_metricPublisher(bsl::make_shared<rmqtestutil::MockMetricPublisher>())
    , d_clientProperties(generateDefaultClientProperties())
    , d_factory(bsl::make_shared<ConnectionFactory>(d_resolver,
                                                    d_timerFactory,
                                                    d_errorCallback,
                                                    d_metricPublisher,
                                                    d_clientProperties,
                                                    d_retryHandler,
                                                    d_heartbeat,
                                                    d_channelFactory))
    , d_callbacks()
    , d_onWriteCompleteCb(bdlf::BindUtil::bind(&Callbacks::onWriteCompleteCb,
                                               &d_callbacks,
                                               rmqamqp::Channel::READY))
    , d_onConnectCb(
          bdlf::BindUtil::bind(&Callbacks::onConnect, &d_callbacks, _1))
    , d_endpoint(new rmqt::SimpleEndpoint("127.0.0.1", TEST_VHOST))
    , d_credentials(new rmqt::PlainCredentials("guest", "guest"))
    {
        EXPECT_CALL(*d_heartbeat, notifyMessageReceived())
            .WillRepeatedly(Return());
        EXPECT_CALL(*d_heartbeat, notifyMessageSent()).WillRepeatedly(Return());

        d_vhostTag.push_back(bsl::pair<bsl::string, bsl::string>(
            rmqamqp::Metrics::VHOST_TAG, TEST_VHOST));
    }

    void SetUp()
    {
        EXPECT_CALL(*d_resolver, asyncConnect(_, _, _, _, _, _))
            .WillRepeatedly(ConnectMockConnection(
                &d_connection, bsl::ref(d_replayFrame), bsl::ref(d_eventLoop)));
    }

    void feedNextFrame() { d_connection->feedNextFrame(); }

    void expectHeader()
    {
        d_replayFrame.pushOutbound(bsl::make_shared<SerializedFrame>(
            static_cast<const uint8_t*>(Constants::PROTOCOL_HEADER),
            Constants::PROTOCOL_HEADER_LENGTH));
    }

    void expectHeaderAndStartFrames(
        const rmqt::FieldTable& clientProperties =
            generateDefaultClientProperties("test-connection"))
    {

        expectHeader();

        const uint16_t channel = 0;

        {
            Frame startFrame;
            rmqt::FieldTable ft;
            ft["foo"] = rmqt::FieldValue(bsl::string("string"));
            bsl::vector<bsl::string> mechanisms;
            mechanisms.push_back("PLAIN");
            mechanisms.push_back("AMQPLAIN");
            bsl::vector<bsl::string> locales;
            locales.push_back("en-US");

            ConnectionStart startMethod(0, 9, ft, mechanisms, locales);
            Framer::makeMethodFrame(
                &startFrame, channel, Method(ConnectionMethod(startMethod)));
            d_replayFrame.pushInbound(
                bsl::make_shared<SerializedFrame>(startFrame));
        }
        {
            ConnectionStartOk startOkMethod(clientProperties,
                                            Constants::AUTHENTICATION_MECHANISM,
                                            bsl::string("\0guest\0guest", 12),
                                            Constants::LOCALE);
            Frame startOkFrame;
            Framer::makeMethodFrame(&startOkFrame,
                                    channel,
                                    Method(ConnectionMethod(startOkMethod)));
            d_replayFrame.pushOutbound(
                bsl::make_shared<SerializedFrame>(startOkFrame));
        }
    }

    void expectOpenFrame()
    {
        const uint16_t channel  = 0;
        const bsl::string vhost = TEST_VHOST;

        {
            ConnectionOpen openMethod(vhost);
            Frame openFrame;
            Framer::makeMethodFrame(
                &openFrame, channel, Method(ConnectionMethod(openMethod)));
            d_replayFrame.pushOutbound(
                bsl::make_shared<SerializedFrame>(openFrame));
        }
    }

    void expectOpenOkFrame()
    {
        const uint16_t channel = 0;

        {
            ConnectionOpenOk openOkMethod;
            Frame openOkFrame;
            Framer::makeMethodFrame(
                &openOkFrame, channel, Method(ConnectionMethod(openOkMethod)));
            d_replayFrame.pushInbound(
                bsl::make_shared<SerializedFrame>(openOkFrame));
        }

        if (d_channelFactory->d_mockReceiveChannelCreated) {
            EXPECT_CALL(*d_receiveChannel, open());
        }
        if (d_channelFactory->d_mockSendChannelCreated) {
            EXPECT_CALL(*d_sendChannel, open());
        }
    }

    void expectOpenFrames()
    {
        expectOpenFrame();
        expectOpenOkFrame();
    }

    void expectTuneFrames()
    {
        const uint16_t channel = 0;
        {
            ConnectionTune tuneMethod(2048, Frame::getMaxFrameSize(), 5);

            Frame tuneFrame;
            Framer::makeMethodFrame(
                &tuneFrame, channel, Method(ConnectionMethod(tuneMethod)));
            d_replayFrame.pushInbound(
                bsl::make_shared<SerializedFrame>(tuneFrame));
        }

        {
            ConnectionTuneOk tuneOkMethod(2048, Frame::getMaxFrameSize(), 5);
            Frame tuneOkFrame;
            Framer::makeMethodFrame(
                &tuneOkFrame, channel, Method(ConnectionMethod(tuneOkMethod)));
            d_replayFrame.pushOutbound(
                bsl::make_shared<SerializedFrame>(tuneOkFrame));
        }
    }

    void expectCloseFrame(bool client = true)
    {
        ConnectionClose closeMethod(
            client ? rmqamqpt::Constants::CONNECTION_FORCED
                   : rmqamqpt::Constants::REPLY_SUCCESS,
            client ? "Closed via management plugin (gtest)"
                   : "Closing Connection");
        Frame closeFrame;
        Framer::makeMethodFrame(
            &closeFrame, 0, Method(ConnectionMethod(closeMethod)));
        client ? d_replayFrame.pushInbound(
                     bsl::make_shared<SerializedFrame>(closeFrame))
               : d_replayFrame.pushOutbound(
                     bsl::make_shared<SerializedFrame>(closeFrame));
    }

    void expectCloseOkFrame(bool client = true)
    {
        Frame closeOkFrame;
        Framer::makeMethodFrame(
            &closeOkFrame, 0, Method(ConnectionMethod(ConnectionCloseOk())));
        client ? d_replayFrame.pushOutbound(
                     bsl::make_shared<SerializedFrame>(closeOkFrame))
               : d_replayFrame.pushInbound(
                     bsl::make_shared<SerializedFrame>(closeOkFrame));
    }

    void expectShutdownCalls()
    {
        EXPECT_CALL(*d_heartbeat, stop()).RetiresOnSaturation();
        if (d_channelFactory->d_mockReceiveChannelCreated) {
            EXPECT_CALL(*d_receiveChannel, reset(false))
                .WillOnce(Return(rmqamqp::Channel::KEEP));
        }
        if (d_channelFactory->d_mockSendChannelCreated) {
            EXPECT_CALL(*d_sendChannel, reset(false))
                .WillOnce(Return(rmqamqp::Channel::KEEP));
        }
    }

    void resetExpectations()
    {
        EXPECT_CALL(*d_heartbeat, stop());
        if (d_channelFactory->d_mockReceiveChannelCreated) {
            EXPECT_CALL(*d_receiveChannel, reset(false))
                .WillOnce(Return(rmqamqp::Channel::KEEP));
        }
        if (d_channelFactory->d_mockSendChannelCreated) {
            EXPECT_CALL(*d_sendChannel, reset(false))
                .WillOnce(Return(rmqamqp::Channel::KEEP));
        }

        // Retry the connection because received unexpected frame
        EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(InvokeArgument<0>());
    }

    void expectFirstHandshakeFrames()
    {
        expectHandshakeFrames();
        EXPECT_CALL(d_callbacks, onConnect(true));
    }

    void expectHandshakeFrames()
    {
        expectHeaderAndStartFrames();
        expectTuneFrames();
        expectOpenFrames();
    }

    void expectMsgFrames(const bsl::shared_ptr<rmqamqp::Message>& msgPtr)
    {
        uint16_t dummyChannel = 1;
        bsl::vector<rmqamqpt::Frame> frames;
        rmqamqp::Framer framer;
        framer.makeFrames(&frames, dummyChannel, *msgPtr);

        for (bsl::vector<rmqamqpt::Frame>::const_iterator it = frames.cbegin();
             it != frames.end();
             ++it) {
            d_replayFrame.pushOutbound(bsl::make_shared<SerializedFrame>(*it));
        }
    }

    bsl::shared_ptr<rmqamqp::Connection>
    createAndStartConnection(const bsl::string& name = "test-connection")
    {
        bsl::shared_ptr<rmqamqp::Connection> conn =
            d_factory->create(d_endpoint, d_credentials, name);
        conn->startFirstConnection(d_onConnectCb);

        return conn;
    }
};

} // namespace

TEST_F(ConnectionTests, SendHeader)
{
    expectHeader();

    bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

    // 1. Expect the AMQP header
    d_eventLoop.run();
    d_eventLoop.restart();

    expectShutdownCalls();

    // 2. Shutdown from incomplete handshake
    d_eventLoop.run();
    EXPECT_CALL(d_callbacks, onConnect(false));
}

TEST_F(ConnectionTests, Handshake)
{
    expectFirstHandshakeFrames();

    bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

    // 1. Handshake
    d_eventLoop.run();

    EXPECT_THAT(d_replayFrame.getLength(), Eq(0));
    expectShutdownCalls();
}

TEST_F(ConnectionTests, ClientProperties)
{
    rmqt::FieldTable overriddenClientProperties =
        generateDefaultClientProperties();
    overriddenClientProperties["FOO"] =
        rmqt::FieldValue(bsl::string("BAR")); // Add one more
    d_factory = bsl::make_shared<ConnectionFactory>(d_resolver,
                                                    d_timerFactory,
                                                    d_errorCallback,
                                                    d_metricPublisher,
                                                    overriddenClientProperties,
                                                    d_retryHandler,
                                                    d_heartbeat,
                                                    d_channelFactory);

    expectHeaderAndStartFrames(
        overriddenClientProperties); // check it's as expected
    expectTuneFrames();
    expectOpenFrame();

    bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

    // 1. Handshake w/ Client properties
    d_eventLoop.run();
    d_eventLoop.restart();

    expectShutdownCalls();

    // 2. Shutdown cleanly
    d_eventLoop.run();
}

TEST_F(ConnectionTests, ClientPropertiesCantOverrideReservedOnes)
{
    rmqt::FieldTable overriddenClientProperties =
        generateDefaultClientProperties("my random connection name");
    overriddenClientProperties["platform"] = rmqt::FieldValue(
        bsl::string("Should get overriden by library")); // Add one more
    overriddenClientProperties["product"] = rmqt::FieldValue(
        bsl::string("Should get overriden by library")); // Add one more
    overriddenClientProperties["version"] = rmqt::FieldValue(
        bsl::string("Should get overriden by library")); // Add one more
    overriddenClientProperties["connection_name"] = rmqt::FieldValue(
        bsl::string("Should get overriden by library")); // Add one more
    d_factory = bsl::make_shared<ConnectionFactory>(d_resolver,
                                                    d_timerFactory,
                                                    d_errorCallback,
                                                    d_metricPublisher,
                                                    overriddenClientProperties,
                                                    d_retryHandler,
                                                    d_heartbeat,
                                                    d_channelFactory);

    expectHeaderAndStartFrames(generateDefaultClientProperties(
        "my real connection name")); // despite setting overrides, the library
                                     // has the final say
    expectTuneFrames();
    expectOpenFrame();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn =
            createAndStartConnection("my real connection name");

        // 1. Handshake up to open with custom client properties
        d_eventLoop.run();
        d_eventLoop.restart();

        expectShutdownCalls();
    }

    // 2. Shutdown cleanly
    d_eventLoop.run();
}

TEST_F(ConnectionTests, GetFrameIncorrectOrder)
{
    expectHeader();

    {
        ConnectionTune tuneMethod(2048, Frame::getMaxFrameSize(), 5);
        Frame tuneFrame;
        Framer::makeMethodFrame(
            &tuneFrame, 0, Method(ConnectionMethod(tuneMethod)));
        d_replayFrame.pushInbound(bsl::make_shared<SerializedFrame>(tuneFrame));
    }

    {
        ConnectionClose closeMethod(Constants::UNEXPECTED_FRAME,
                                    "Unexpected frame",
                                    Constants::CONNECTION,
                                    Constants::CONNECTION_TUNE);
        Frame closeFrame;
        Framer::makeMethodFrame(
            &closeFrame, 0, Method(ConnectionMethod(closeMethod)));
        d_replayFrame.pushOutbound(
            bsl::make_shared<SerializedFrame>(closeFrame));

        expectCloseOkFrame(false); // from the server
    }
    resetExpectations();

    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Process bad handshake, disconnect, reconnect successfully
        d_eventLoop.run();
        d_eventLoop.restart();

        //        Mock::VerifyAndClearExpectations(d_heartbeat.get());
        expectShutdownCalls();
    }

    // 2. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, GracefulClose)
{
    // 1. Setup connection
    expectFirstHandshakeFrames();

    bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

    // 1. Setup connection
    d_eventLoop.run();
    d_eventLoop.restart();

    expectCloseFrame(false);
    expectCloseOkFrame(false);

    bool invoked = false;
    conn->close(bdlf::BindUtil::bind(&gracefulCloseHandler, bsl::ref(invoked)));

    // 2. Process shutdown
    d_eventLoop.run();

    EXPECT_THAT(conn->state(), Eq(rmqamqp::Connection::DISCONNECTED));

    EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

    EXPECT_TRUE(invoked);
}

TEST_F(ConnectionTests, AddReceiveChannelWhenConnected)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        EXPECT_CALL(*d_channelFactory,
                    createReceiveChannel(_, _, _, _, _, _, _, _, _))
            .WillOnce(DoAll(ReceiveChannelCreated(bsl::ref(d_channelFactory)),
                            Return(d_receiveChannel)));

        EXPECT_CALL(*d_receiveChannel, open());

        rmqt::ConsumerConfig consumerConfig(
            rmqt::ConsumerConfig::generateConsumerTag(), 5);
        bsl::shared_ptr<rmqamqp::ReceiveChannel> channel =
            conn->createReceiveChannel(rmqt::Topology(),
                                       d_retryHandlerChannel,
                                       consumerConfig,
                                       d_ackQueue);

        // Checks receive channel is shutdown correctly
        expectShutdownCalls();
    }

    // 2. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, AddSendChannelWhenConnected)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        EXPECT_CALL(*d_channelFactory,
                    createSendChannel(_, _, _, _, _, _, _, _))
            .WillOnce(DoAll(SendChannelCreated(bsl::ref(d_channelFactory)),
                            Return(d_sendChannel)));

        EXPECT_CALL(*d_sendChannel, open());

        bsl::shared_ptr<rmqamqp::SendChannel> channel = conn->createSendChannel(
            rmqt::Topology(),
            bsl::make_shared<rmqt::Exchange>("exchange"),
            d_retryHandlerChannel);

        expectShutdownCalls();
    }

    // 2. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, DisconnectTriggersChannelReset)
{
    expectFirstHandshakeFrames();
    {

        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        EXPECT_CALL(*d_channelFactory,
                    createReceiveChannel(_, _, _, _, _, _, _, _, _))
            .WillOnce(DoAll(ReceiveChannelCreated(bsl::ref(d_channelFactory)),
                            Return(d_receiveChannel)));

        EXPECT_CALL(*d_receiveChannel, open());

        rmqt::ConsumerConfig consumerConfig(
            rmqt::ConsumerConfig::generateConsumerTag(), 5);
        bsl::shared_ptr<rmqamqp::ReceiveChannel> channel =
            conn->createReceiveChannel(rmqt::Topology(),
                                       d_retryHandlerChannel,
                                       consumerConfig,
                                       d_ackQueue);

        // 2. Channel creation
        d_eventLoop.run();
        d_eventLoop.restart();

        expectCloseFrame(); // load up a close from server
        expectCloseOkFrame();

        // This expects a channel reset if a channel was opened
        resetExpectations();

        expectHandshakeFrames();

        feedNextFrame(); // Queue server close frame

        // 3. Process server close, disconnect & reconnect
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));
        expectShutdownCalls();
    }

    // 4. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, handleAsyncWriteMessageSuccess)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        rmqamqp::Channel::AsyncWriteCallback writeMsg;
        EXPECT_CALL(*d_channelFactory,
                    createSendChannel(_, _, _, _, _, _, _, _))
            .WillOnce(DoAll(SaveArg<2>(&writeMsg),
                            SendChannelCreated(bsl::ref(d_channelFactory)),
                            Return(d_sendChannel)));

        bsl::shared_ptr<rmqamqp::SendChannel> channel = conn->createSendChannel(
            rmqt::Topology(),
            bsl::make_shared<rmqt::Exchange>("exchange"),
            d_retryHandlerChannel);

        // 3. Send a message

        const size_t total_data = 50;
        bsl::shared_ptr<rmqamqp::Message> msgPtr =
            bsl::make_shared<rmqamqp::Message>(rmqamqp::Message(rmqt::Message(
                bsl::make_shared<bsl::vector<uint8_t> >(total_data))));
        expectMsgFrames(msgPtr);

        EXPECT_CALL(d_callbacks, onWriteCompleteCb(_));
        writeMsg(msgPtr, d_onWriteCompleteCb);

        // 2. Setup channel, send message
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectShutdownCalls();
    }

    // 3. Shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, handleAsyncWriteMessageFail)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        rmqamqp::Channel::AsyncWriteCallback writeMsg;
        EXPECT_CALL(*d_channelFactory,
                    createSendChannel(_, _, _, _, _, _, _, _))
            .WillOnce(DoAll(SaveArg<2>(&writeMsg),
                            SendChannelCreated(bsl::ref(d_channelFactory)),
                            Return(d_sendChannel)));

        bsl::shared_ptr<rmqamqp::SendChannel> channel = conn->createSendChannel(
            rmqt::Topology(),
            bsl::make_shared<rmqt::Exchange>("exchange"),
            d_retryHandlerChannel);

        const size_t total_data = 50;
        bsl::shared_ptr<rmqamqp::Message> msgPtr =
            bsl::make_shared<rmqamqp::Message>(rmqamqp::Message(rmqt::Message(
                bsl::make_shared<bsl::vector<uint8_t> >(total_data))));
        expectMsgFrames(msgPtr);

        EXPECT_CALL(*d_connection, asyncWrite(_, _))
            .WillOnce(OnError(bsl::ref(d_connection)));
        EXPECT_CALL(d_callbacks, onWriteCompleteCb(_)).Times(0);
        EXPECT_THAT(d_replayFrame.getLength(), Ne(0));
        EXPECT_CALL(*d_heartbeat, stop()).Times(2);
        EXPECT_CALL(*d_sendChannel, reset(false))
            .Times(2)
            .WillRepeatedly(Return(rmqamqp::Channel::KEEP));
        EXPECT_CALL(*d_retryHandler, retry(_));
        writeMsg(msgPtr, d_onWriteCompleteCb);

        // 3. Process part of message response, expect error shutdown
        d_eventLoop.run();
    }
}

TEST_F(ConnectionTests, ReopenConnectionIfServerCloses)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectCloseFrame();
        expectCloseOkFrame();

        resetExpectations();
        expectHandshakeFrames();

        feedNextFrame(); // Queue Server Close Frame

        // 2. Send server close, process close & reconnect
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));
        EXPECT_THAT(conn->state(), Eq(rmqamqp::Connection::CONNECTED));
        expectShutdownCalls();
    }

    // 3. Process shutdown
    d_eventLoop.run();
}

class ConnectionHeartbeatTests : public ConnectionTests {};

TEST_F(ConnectionHeartbeatTests, SendHeartbeat)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        d_replayFrame.pushOutbound(
            bsl::make_shared<SerializedFrame>(Framer::makeHeartbeatFrame()));

        d_heartbeat->d_sendHeartbeat(Framer::makeHeartbeatFrame());

        // 2. Process heartbeat
        d_eventLoop.run();
        d_eventLoop.restart();

        expectShutdownCalls();
    }

    // 3. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionHeartbeatTests, KillConnection)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        resetExpectations();

        expectHandshakeFrames();
        d_heartbeat->d_killConnection();

        // 2. HeartbeatManager kills connection & connection re-opens
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectShutdownCalls();
    }

    // 3. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, PublishHeartbeatTimeoutMetric)
{
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("disconnect_events"), _, d_vhostTag))
        .Times(AnyNumber());

    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        resetExpectations();

        expectHandshakeFrames();

        EXPECT_CALL(
            *d_metricPublisher,
            publishCounter(bsl::string("heartbeat_timeouts"), _, d_vhostTag));

        d_heartbeat->d_killConnection();

        // 2. HeartbeatManager kills connection & connection re-opens
        d_eventLoop.run();
        d_eventLoop.restart();

        Mock::VerifyAndClearExpectations(&*d_metricPublisher);

        expectShutdownCalls();
    }

    // 3. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, PublishConnectTimeMetric)
{
    EXPECT_CALL(
        *d_metricPublisher,
        publishDistribution(bsl::string("connect_time"), _, d_vhostTag));

    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        expectShutdownCalls();
    }

    // 2. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, PublishDisconnectEventMetric)
{
    expectFirstHandshakeFrames();

    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("disconnect_events"), _, d_vhostTag))
        .WillRepeatedly(Return());

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        expectCloseFrame(false);
        expectCloseOkFrame(false);

        conn->close(&noopCloseHandler);
    }

    // 2. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, PublishReconnectTimeMetric)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_CALL(*d_channelFactory,
                    createReceiveChannel(_, _, _, _, _, _, _, _, _))
            .WillOnce(DoAll(ReceiveChannelCreated(bsl::ref(d_channelFactory)),
                            Return(d_receiveChannel)));

        rmqt::ConsumerConfig consumerConfig(
            rmqt::ConsumerConfig::generateConsumerTag(), 5);
        bsl::shared_ptr<rmqamqp::ReceiveChannel> channel =
            conn->createReceiveChannel(rmqt::Topology(),
                                       d_retryHandlerChannel,
                                       consumerConfig,
                                       d_ackQueue);

        // Close

        expectCloseFrame(); // load up a close from server
        expectCloseOkFrame();

        EXPECT_CALL(
            *d_metricPublisher,
            publishDistribution(bsl::string("reconnect_time"), _, d_vhostTag));

        resetExpectations();
        expectHandshakeFrames(); // after reopen
        feedNextFrame();         // Queue up server close frame

        // 2. Trigger server close, process close & re-open
        d_eventLoop.run();
        d_eventLoop.restart();

        expectShutdownCalls();
    }

    // 3. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionTests, NoRetryIfDead)
{
    expectFirstHandshakeFrames();

    // Calling this should no-op after connection is destructed
    bsl::function<void()> retryFunc;
    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Setup connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectCloseFrame();
        expectCloseOkFrame();

        EXPECT_CALL(*d_heartbeat, stop()).Times(2);
        if (d_channelFactory->d_mockReceiveChannelCreated) {
            EXPECT_CALL(*d_receiveChannel, reset(false))
                .WillOnce(Return(rmqamqp::Channel::KEEP));
        }
        if (d_channelFactory->d_mockSendChannelCreated) {
            EXPECT_CALL(*d_sendChannel, reset(false))
                .WillOnce(Return(rmqamqp::Channel::KEEP));
        }

        // Retry the connection because received unexpected frame
        EXPECT_CALL(*d_retryHandler, retry(_)).WillOnce(SaveArg<0>(&retryFunc));

        feedNextFrame(); // Queue up Server Close Frame

        // 2. Trigger server close, process close & re-open
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));
    }

    // 3. Process shutdown
    d_eventLoop.run();

    // Test that the retry function won't cause a segfault/memory violation when
    // called
    retryFunc();
}

TEST_F(ConnectionTests, ConnectionDiesWhenDestructedMidConnection)
{
    bsl::function<void()> connectFunc;
    EXPECT_CALL(*d_resolver, asyncConnect(_, _, _, _, _, _))
        .WillRepeatedly(DoAll(SaveArg<4>(&connectFunc),
                              ConnectMockConnection(&d_connection,
                                                    bsl::ref(d_replayFrame),
                                                    bsl::ref(d_eventLoop))));

    bsl::weak_ptr<rmqamqp::Connection> weakConn;

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        weakConn = conn;
    }

    EXPECT_FALSE(weakConn.lock());
}

class TuneParamNegotiationTests : public ConnectionTests {};

TEST_F(TuneParamNegotiationTests, ServerSendsZeroNegotiateToOurValue)
{
    expectHeaderAndStartFrames();
    const uint16_t channel = 0;

    {
        ConnectionTune tuneMethod(0, 0, 0);

        Frame tuneFrame;
        Framer::makeMethodFrame(
            &tuneFrame, channel, Method(ConnectionMethod(tuneMethod)));
        d_replayFrame.pushInbound(bsl::make_shared<SerializedFrame>(tuneFrame));
    }

    {
        // k_MAX_HEARTBEAT_TIMEOUT_SEC and k_MAX_CHANNEL_NUM
        ConnectionTuneOk tuneOkMethod(65535, Frame::getMaxFrameSize(), 60);
        Frame tuneOkFrame;
        Framer::makeMethodFrame(
            &tuneOkFrame, channel, Method(ConnectionMethod(tuneOkMethod)));
        d_replayFrame.pushOutbound(
            bsl::make_shared<SerializedFrame>(tuneOkFrame));
    }

    expectOpenFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Start connection, with custom tune
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectShutdownCalls();
    }

    // 2. Process shutdown
    d_eventLoop.run();
}

TEST_F(TuneParamNegotiationTests, ServerSendsHighValueNegotiateToOurValue)
{
    expectHeaderAndStartFrames();
    const uint16_t channel = 0;

    {
        ConnectionTune tuneMethod(bsl::numeric_limits<uint16_t>::max(),
                                  bsl::numeric_limits<uint32_t>::max(),
                                  bsl::numeric_limits<uint16_t>::max());

        Frame tuneFrame;
        Framer::makeMethodFrame(
            &tuneFrame, channel, Method(ConnectionMethod(tuneMethod)));
        d_replayFrame.pushInbound(bsl::make_shared<SerializedFrame>(tuneFrame));
    }

    {
        // k_MAX_HEARTBEAT_TIMEOUT_SEC and k_MAX_CHANNEL_NUM
        ConnectionTuneOk tuneOkMethod(65535, Frame::getMaxFrameSize(), 60);
        Frame tuneOkFrame;
        Framer::makeMethodFrame(
            &tuneOkFrame, channel, Method(ConnectionMethod(tuneOkMethod)));
        d_replayFrame.pushOutbound(
            bsl::make_shared<SerializedFrame>(tuneOkFrame));
    }

    expectOpenFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Start connection, with custom tune
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectShutdownCalls();
    }

    // 2. Process shutdown
    d_eventLoop.run();
}

TEST_F(TuneParamNegotiationTests, ServerSendsLowValueNegotiateToTheirValue)
{
    expectHeaderAndStartFrames();
    const uint16_t channel = 0;

    {
        ConnectionTune tuneMethod(100, 4096, 1);

        Frame tuneFrame;
        Framer::makeMethodFrame(
            &tuneFrame, channel, Method(ConnectionMethod(tuneMethod)));
        d_replayFrame.pushInbound(bsl::make_shared<SerializedFrame>(tuneFrame));
    }

    {
        // k_MAX_HEARTBEAT_TIMEOUT_SEC and k_MAX_CHANNEL_NUM
        ConnectionTuneOk tuneOkMethod(100, 4096, 1);
        Frame tuneOkFrame;
        Framer::makeMethodFrame(
            &tuneOkFrame, channel, Method(ConnectionMethod(tuneOkMethod)));
        d_replayFrame.pushOutbound(
            bsl::make_shared<SerializedFrame>(tuneOkFrame));
    }

    expectOpenFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Start connection, with custom tune
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectShutdownCalls();
    }

    // 2. Process shutdown
    d_eventLoop.run();
}

class ConnectionHungTests : public ConnectionTests {};

TEST_F(ConnectionHungTests, ConnectNormally)
{
    expectFirstHandshakeFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Start connection
        d_eventLoop.run();
        d_eventLoop.restart();

        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));
        d_timerFactory->step_time(bsls::TimeInterval(120));

        expectShutdownCalls();
    }

    // 2. Process normal shutdown w/ no hung connection
    d_eventLoop.run();
}

TEST_F(ConnectionHungTests, KillConnectionAfterStart)
{
    expectHeaderAndStartFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Start partial handshake
        d_eventLoop.run();
        d_eventLoop.restart();

        resetExpectations();
        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectFirstHandshakeFrames();
        d_timerFactory->step_time(bsls::TimeInterval(120));

        // 2. Process hung timer event & successful reconnect
        d_eventLoop.run();
        d_eventLoop.restart();

        expectShutdownCalls();
    }

    // 3. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionHungTests, KillConnectionAfterTune)
{
    expectHeaderAndStartFrames();

    {
        expectTuneFrames();
        expectOpenFrame();

        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Start partial handshake
        d_eventLoop.run();
        d_eventLoop.restart();

        resetExpectations();
        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectFirstHandshakeFrames();
        d_timerFactory->step_time(bsls::TimeInterval(120));

        // 2. Process hung timer event & successful reconnect
        d_eventLoop.run();
        d_eventLoop.restart();

        expectShutdownCalls();
    }

    // 3. Process shutdown
    d_eventLoop.run();
}

TEST_F(ConnectionHungTests, HungConnectionSendsMetric)
{
    // Since we're expecting a metric we need to handle some other metrics too:
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("disconnect_events"), _, d_vhostTag))
        .Times(AnyNumber());

    expectHeaderAndStartFrames();

    {
        bsl::shared_ptr<rmqamqp::Connection> conn = createAndStartConnection();

        // 1. Start partial handshake
        d_eventLoop.run();
        d_eventLoop.restart();

        resetExpectations();
        EXPECT_THAT(d_replayFrame.getLength(), Eq(0));

        expectFirstHandshakeFrames();
        EXPECT_CALL(*d_metricPublisher,
                    publishCounter(bsl::string("hung_connection_reset"), _, _));

        d_timerFactory->step_time(bsls::TimeInterval(120));

        // 2. Process hung timer event, metric publish, & successful reconnect
        d_eventLoop.run();
        d_eventLoop.restart();

        expectShutdownCalls();
    }

    // 3. Process shutdown
    d_eventLoop.run();
}
