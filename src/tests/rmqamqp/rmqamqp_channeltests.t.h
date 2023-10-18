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

#ifndef INCLUDED_RMQAMQP_CHANNELTESTS_T
#define INCLUDED_RMQAMQP_CHANNELTESTS_T

#include <rmqamqp_channel.h>
#include <rmqamqp_framer.h>
#include <rmqamqpt_method.h>
#include <rmqio_retryhandler.h>
#include <rmqt_message.h>
#include <rmqt_topology.h>
#include <rmqtestutil_mockretryhandler.t.h>

#include <bdlf_bind.h>
#include <bsl_functional.h>
#include <bsl_iostream.h>
#include <bsl_memory.h>

#include <bsl_vector.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

namespace BloombergLP {
namespace rmqamqp {

MATCHER_P(MessageEq, expected, "")
{
    uint16_t irrelevant = 123;
    bsl::vector<rmqamqpt::Frame> actualFrames, expectedFrames;
    rmqamqp::Framer framer;
    framer.setMaxFrameSize(100);
    framer.makeFrames(&actualFrames, irrelevant, arg);
    framer.makeFrames(&expectedFrames, irrelevant, expected);
    return (actualFrames == expectedFrames);
}

MATCHER_P(MethodMsgTypeEq,
          expected,
          "Message is not of method type: " + PrintToString(expected))
{
    return arg.template is<rmqamqpt::Method>() &&
           rmqamqpt::Method::Util::typeMatch(
               arg.template the<rmqamqpt::Method>(), expected);
}

class Callback {
  public:
    virtual void
    onAsyncWrite(const bsl::shared_ptr<rmqamqp::Message>&,
                 const rmqio::Connection::SuccessWriteCallback&) = 0;
    virtual void onNewMessage(const rmqt::Message&)              = 0;
    virtual void onHungTimerCallback()                           = 0;
};

class MockCallback : public Callback {
  public:
    MOCK_METHOD2(onAsyncWrite,
                 void(const bsl::shared_ptr<rmqamqp::Message>&,
                      const rmqio::Connection::SuccessWriteCallback&));
    MOCK_METHOD1(onNewMessage, void(const rmqt::Message&));

    MOCK_METHOD0(onHungTimerCallback, void());
};

using namespace bdlf::PlaceHolders;

class ChannelTests : public ::testing::Test {
  public:
    MockCallback d_callback;
    StrictMock<rmqamqp::Channel::AsyncWriteCallback> d_onAsyncWrite;
    rmqt::Topology d_topology;
    bsl::shared_ptr<rmqtestutil::MockRetryHandler> d_retryHandler;
    rmqt::QueueHandle d_queue;
    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
    StrictMock<rmqamqp::Channel::HungChannelCallback> d_connErrorCb;

    ChannelTests()
    : d_callback()
    , d_onAsyncWrite(
          bdlf::BindUtil::bind(&Callback::onAsyncWrite, &d_callback, _1, _2))
    , d_topology()
    , d_retryHandler(new rmqtestutil::MockRetryHandler())
    , d_queue()
    , d_timerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    , d_connErrorCb(
          bdlf::BindUtil::bind(&Callback::onHungTimerCallback, &d_callback))
    {
        d_topology.queues.push_back(bsl::make_shared<rmqt::Queue>(
            "test-queue", false, false, false, rmqt::FieldTable()));
        d_queue = d_topology.queues[0];
    }

    void queueDeclareReply(rmqamqp::Channel& ch)
    {
        rmqamqpt::QueueDeclareOk declareOk("test-queue", 0, 0);
        ch.processReceived(rmqamqp::Message(
            rmqamqpt::Method(rmqamqpt::QueueMethod(declareOk))));
    }

    void channelOpenExpectation()
    {
        EXPECT_CALL(
            d_callback,
            onAsyncWrite(
                Pointee(MessageEq(rmqamqp::Message(rmqamqpt::Method(
                    rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen()))))),
                _))
            .WillOnce(InvokeArgument<1>());
    }

    void openExpectations()
    {
        channelOpenExpectation();

        EXPECT_CALL(
            d_callback,
            onAsyncWrite(Pointee(MethodMsgTypeEq(rmqamqpt::Method(
                             rmqamqpt::QueueMethod(rmqamqpt::QueueDeclare())))),
                         _))
            .WillOnce(InvokeArgument<1>()); // queue declare
    }

    void openOkReply(rmqamqp::Channel& ch)
    {
        ch.processReceived(rmqamqp::Message(rmqamqpt::Method(
            rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpenOk()))));
    }

    void openAndSendTopology(rmqamqp::Channel& ch)
    {
        openExpectations();
        ch.open();
        openOkReply(ch);
    }
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
