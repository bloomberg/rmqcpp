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

#include <rmqa_connectionmonitor.h>

#include <rmqamqp_channelcontainer.h>
#include <rmqamqp_channelmap.h>
#include <rmqamqp_messagestore.h>
#include <rmqt_message.h>
#include <rmqtestutil_mockchannel.t.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bdlf_bind.h>

#include <bsl_memory.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace rmqa;
using namespace ::testing;

class MockCallback {
  public:
    MOCK_METHOD1(cb, void(const rmqamqp::MessageStore<rmqt::Message>::Entry&));
};

class MockConnection : public rmqamqp::ChannelContainer {
  public:
    MOCK_CONST_METHOD0(channelMap, const rmqamqp::ChannelMap&());
    MOCK_CONST_METHOD0(connectionDebugName, bsl::string());
};

class ConnectionMonitorTests : public Test {
  public:
    MockCallback d_cb;
    bsl::shared_ptr<MockConnection> d_connection;
    bsl::shared_ptr<rmqtestutil::MockReceiveChannel> d_channel;
    bsls::TimeInterval d_timeout;
    bsl::shared_ptr<ConnectionMonitor> d_monitor;
    bdlt::CurrentTime::CurrentTimeCallback d_prev;
    static bsls::TimeInterval s_time;
    rmqt::Message d_message;
    rmqamqp::MessageStore<rmqt::Message>::Entry d_entry;
    rmqamqp::MessageStore<rmqt::Message>::MessageList d_messageVector;
    rmqamqp::ChannelMap d_channelMap;

    ConnectionMonitorTests()
    : d_connection(bsl::make_shared<MockConnection>())
    , d_channel(bsl::make_shared<rmqtestutil::MockReceiveChannel>(
          bsl::make_shared<rmqt::ConsumerAckQueue>()))
    , d_timeout(60)
    , d_monitor(bsl::make_shared<ConnectionMonitor>(
          d_timeout,
          bdlf::BindUtil::bind(&MockCallback::cb,
                               &d_cb,
                               bdlf::PlaceHolders::_1)))
    , d_prev(bdlt::CurrentTime::setCurrentTimeCallback(time))
    {
        s_time = bsls::TimeInterval(0);

        bdlt::Datetime dt(1970, 1, 1);

        bsl::string json = "[5, 3, 1]";
        d_message = rmqt::Message(bsl::make_shared<bsl::vector<uint8_t> >(
            json.cbegin(), json.cend()));
        d_entry   = rmqamqp::MessageStore<rmqt::Message>::Entry(
            1, bsl::pair<rmqt::Message, bdlt::Datetime>(d_message, dt));
    }

    ~ConnectionMonitorTests()
    {
        bdlt::CurrentTime::setCurrentTimeCallback(d_prev);
    }

    static bsls::TimeInterval time();
};

bsls::TimeInterval ConnectionMonitorTests::s_time(0);

bsls::TimeInterval ConnectionMonitorTests::time()
{
    return ConnectionMonitorTests::s_time;
}

TEST_F(ConnectionMonitorTests, Breathing) {}

TEST_F(ConnectionMonitorTests, HungMessage)
{
    d_messageVector.push_back(d_entry);
    d_channelMap.associateChannel(
        1, bsl::shared_ptr<rmqamqp::ReceiveChannel>(d_channel));
    d_monitor->addConnection(d_connection);

    EXPECT_CALL(*d_connection, channelMap())
        .WillRepeatedly(ReturnRef(d_channelMap));
    EXPECT_CALL(*d_channel, getMessagesOlderThan(_))
        .WillOnce(Return(d_messageVector));
    EXPECT_CALL(d_cb, cb(d_entry));

    s_time += d_timeout;
    d_monitor->run();
}

TEST_F(ConnectionMonitorTests, MessageNotYetHung)
{
    d_channelMap.associateChannel(
        1, bsl::shared_ptr<rmqamqp::ReceiveChannel>(d_channel));
    d_monitor->addConnection(d_connection);

    EXPECT_CALL(*d_connection, channelMap())
        .WillRepeatedly(ReturnRef(d_channelMap));
    EXPECT_CALL(*d_channel, getMessagesOlderThan(_))
        .WillRepeatedly(Return(d_messageVector));
    EXPECT_CALL(d_cb, cb(_)).Times(0);

    bsls::TimeInterval notYet = d_timeout - bsls::TimeInterval(1);
    s_time += notYet;
    d_monitor->run();
}

TEST_F(ConnectionMonitorTests, HungMessageTwice)
{
    d_messageVector.push_back(d_entry);
    d_channelMap.associateChannel(
        1, bsl::shared_ptr<rmqamqp::ReceiveChannel>(d_channel));
    d_monitor->addConnection(d_connection);

    EXPECT_CALL(*d_connection, channelMap())
        .WillRepeatedly(ReturnRef(d_channelMap));
    EXPECT_CALL(*d_channel, getMessagesOlderThan(_))
        .WillOnce(Return(d_messageVector));
    EXPECT_CALL(d_cb, cb(d_entry));

    s_time += d_timeout;
    d_monitor->run();

    Mock::VerifyAndClearExpectations(&d_channel);
    Mock::VerifyAndClearExpectations(&d_cb);

    EXPECT_CALL(*d_channel, getMessagesOlderThan(_))
        .WillOnce(Return(d_messageVector));
    EXPECT_CALL(d_cb, cb(d_entry));

    s_time += d_timeout;
    d_monitor->run();
}

TEST_F(ConnectionMonitorTests, ConnectionExpired)
{
    d_messageVector.push_back(d_entry);
    d_channelMap.associateChannel(
        1, bsl::shared_ptr<rmqamqp::ReceiveChannel>(d_channel));
    d_monitor->addConnection(d_connection);

    EXPECT_CALL(*d_connection, channelMap())
        .WillRepeatedly(ReturnRef(d_channelMap));
    EXPECT_CALL(*d_channel, getMessagesOlderThan(_))
        .WillOnce(Return(d_messageVector));
    EXPECT_CALL(d_cb, cb(d_entry));

    s_time += d_timeout;
    d_monitor->run();

    Mock::VerifyAndClearExpectations(&d_channel);
    Mock::VerifyAndClearExpectations(&d_cb);

    d_connection.reset();

    // connection is gone, don't expect any more checks
    EXPECT_CALL(d_cb, cb(d_entry)).Times(0);

    s_time += d_timeout;
    d_monitor->run();
}

TEST_F(ConnectionMonitorTests, FetchConnectionInfo)
{
    EXPECT_EQ(d_monitor->fetchAliveConnectionInfo()
                  ->aliveConnectionChannelInfo.size(),
              0);

    d_messageVector.push_back(d_entry);
    d_channelMap.associateChannel(
        1, bsl::shared_ptr<rmqamqp::ReceiveChannel>(d_channel));
    d_monitor->addConnection(d_connection);

    EXPECT_CALL(*d_connection, channelMap())
        .WillRepeatedly(ReturnRef(d_channelMap));
    EXPECT_CALL(*d_connection, connectionDebugName());

    bsl::shared_ptr<ConnectionMonitor::AliveConnectionInfo> info =
        d_monitor->fetchAliveConnectionInfo();

    EXPECT_EQ(info->aliveConnectionChannelInfo.size(), 1);
    EXPECT_EQ(info->aliveConnectionChannelInfo[0].second.size(), 1);
}
