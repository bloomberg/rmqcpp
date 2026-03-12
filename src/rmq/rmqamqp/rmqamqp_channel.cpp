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

#include <rmqamqp_channel.h>

#include <rmqamqp_framer.h>
#include <rmqamqp_metrics.h>
#include <rmqamqp_topologymerger.h>
#include <rmqamqpt_channelclose.h>
#include <rmqamqpt_channelcloseok.h>
#include <rmqamqpt_channelflow.h>
#include <rmqamqpt_channelflowok.h>
#include <rmqamqpt_channelmethod.h>
#include <rmqamqpt_channelopen.h>
#include <rmqamqpt_channelopenok.h>
#include <rmqamqpt_method.h>
#include <rmqamqpt_queuemethod.h>
#include <rmqp_metricpublisher.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bdlt_currenttime.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.CHANNEL")

void noopWriteHandler() {}

void callBothMakers(const rmqt::Future<>::Maker& maker1,
                    const rmqt::Future<>::Maker& maker2,
                    const rmqt::Result<>& res)
{
    maker1(res);
    maker2(res);
}
} // namespace

const int Channel::k_HUNG_CHANNEL_TIMER_SEC = 60;

Channel::Channel(const rmqt::Topology& topology,
                 const AsyncWriteCallback& onAsyncWrite,
                 const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
                 const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
                 const bsl::string& vhost,
                 const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
                 const HungChannelCallback& connErrorCb)
: d_vhostName(vhost)
, d_flow(true)
, d_metricPublisher(metricPublisher)
, d_vhostTags(
      1,
      bsl::pair<bsl::string, bsl::string>(Metrics::VHOST_TAG, d_vhostName))
, d_vhostAndChannelTags()
, d_hungProgressTimer(hungProgressTimer)
, d_state(CLOSED)
, d_topology(topology)
, d_topologyTransformer()
, d_updateQueue()
, d_onAsyncWrite(onAsyncWrite)
, d_retryHandler(retryHandler)
, d_permanentlyClosing(false)
, d_declareTopologyStartTime()
, d_readyMade()
, d_connErrorCb(connErrorCb)
{
}

class Channel::ChannelMethodProcessor {
    Channel& d_channel;

  public:
    typedef Channel::CleanupIndicator ResultType;

    ChannelMethodProcessor(Channel& channel)
    : d_channel(channel)
    {
    }

    ResultType operator()(const rmqamqpt::ChannelOpenOk&) const
    {
        d_channel.d_hungProgressTimer->reset(
            bsls::TimeInterval(k_HUNG_CHANNEL_TIMER_SEC));
        if (d_channel.d_state != CHANNEL_OPEN_SENT) {
            BALL_LOG_ERROR << "Received Channel OPEN-OK method "
                              "when channel is in "
                           << d_channel.d_state
                           << " state, but expected state is "
                           << CHANNEL_OPEN_SENT;
            d_channel.close(rmqamqpt::Constants::UNEXPECTED_FRAME,
                            "Unexpected frame",
                            rmqamqpt::ChannelMethod::CLASS_ID,
                            rmqamqpt::ChannelOpenOk::METHOD_ID);
            return KEEP;
        }

        d_channel.updateState(DECLARING_TOPOLOGY);
        d_channel.declareTopology();
        return KEEP;
    }

    ResultType operator()(const rmqamqpt::ChannelFlow& channelFlow) const
    {
        if (channelFlow.activeFlow()) {
            BALL_LOG_INFO << "Received Channel.Flow(true)";
            d_channel.d_flow = true;
            d_channel.onFlowAllowed();
            d_channel.writeMessage(
                Message(rmqamqpt::Method(
                    rmqamqpt::ChannelMethod(rmqamqpt::ChannelFlowOk(true)))),
                &noopWriteHandler);
        }
        else {
            BALL_LOG_INFO << "Received Channel.Flow(false)";
            d_channel.d_flow = false;
            d_channel.writeMessage(
                Message(rmqamqpt::Method(
                    rmqamqpt::ChannelMethod(rmqamqpt::ChannelFlowOk(false)))),
                &noopWriteHandler);
        }
        return KEEP;
    }

    ResultType operator()(const rmqamqpt::ChannelClose& closeMethod) const
    {
        BALL_LOG_INFO << "Received ChannelClose from server: " << closeMethod;

        rmqamqpt::ChannelCloseOk closeOkMethod;
        d_channel.writeMessage(
            Message(rmqamqpt::Method(rmqamqpt::ChannelMethod(closeOkMethod))),
            CLOSED);

        if (closeMethod.classId() || closeMethod.methodId()) {
            (d_channel.d_retryHandler->errorCallback())(
                "Channel error " + closeMethod.replyText(),
                closeMethod.replyCode());
        }

        return KEEP;
    }

    ResultType operator()(const rmqamqpt::ChannelCloseOk&) const
    {
        if (d_channel.d_state != CHANNEL_CLOSE_SENT) {
            BALL_LOG_ERROR << "Received channel CLOSE-OK method from server "
                              "without sending CLOSE method";
        }

        if (d_channel.d_permanentlyClosing) {
            return CLEANUP;
        }

        d_channel.reset(true);
        return KEEP;
    }

    template <typename T>
    ResultType operator()(const T& method) const
    {
        BALL_LOG_ERROR << "Unhandled Channel Method: " << method;
        return KEEP;
    }

    ResultType operator()(const BloombergLP::bslmf::Nil) const
    {
        BALL_LOG_ERROR << "This should never happen";
        return KEEP;
    }
};

void Channel::processTopologyMethod(const rmqamqpt::Method& method)
{
    if (d_state != DECLARING_TOPOLOGY && d_state != READY &&
        d_state != AWAITING_REPLY) {
        BALL_LOG_ERROR << "Received topology method " << method << " in state "
                       << d_state;
        return;
    }

    if (!d_topologyTransformer.first) {
        BALL_LOG_ERROR << "TopologyTransformer is null";
        close(rmqamqpt::Constants::CHANNEL_ERROR,
              "Error occurred, while declaring topology");
        if (d_topologyTransformer.second) {
            d_topologyTransformer.second(
                rmqt::Result<>("Error occurred, while declaring topology"));
        }
        d_topologyTransformer.first.reset();
        return;
    }

    if (d_state == DECLARING_TOPOLOGY) {
        // Detect hung topology declaration progress, but ignore for topology
        // updates, which happen in state == READY, because a separate timeout
        // is passed up to the caller for these - tearing down the whole
        // connection is possibly not the best outcome.
        // Perhaps this could be configurable in future.

        d_hungProgressTimer->reset(
            bsls::TimeInterval(k_HUNG_CHANNEL_TIMER_SEC));
    }

    const bool status =
        d_topologyTransformer.first->processReplyMessage(Message(method));

    if (!status) {
        BALL_LOG_ERROR << "TopologyTransformer failed to process method "
                       << method;
        close(rmqamqpt::Constants::CHANNEL_ERROR,
              "Error occurred, while declaring topology");
        if (d_topologyTransformer.second) {
            d_topologyTransformer.second(
                rmqt::Result<>("Error occurred, while declaring topology"));
        }
        d_topologyTransformer.first.reset();
        return;
    }

    if (d_topologyTransformer.first->isDone()) {
        if (d_state != DECLARING_TOPOLOGY) {
            if (d_updateQueue.empty()) {
                BALL_LOG_ERROR
                    << "Received unexpected topology method from broker: "
                    << method
                    << ". All previous topology declarations/updates have been "
                       "acknowledged by the broker. This is likely an issue "
                       "with the RabbitMQ broker - please reach out for "
                       "support.";
                return;
            }
            TopologyMerger::merge(d_topology, d_updateQueue.front().first);
            d_updateQueue.pop();
        }
        if (d_topologyTransformer.second) {
            d_topologyTransformer.second(rmqt::Result<>());
        }
        d_topologyTransformer.first.reset();
        // kick off outstanding updates if there are any
        sendNextUpdate();
    }
}

void Channel::sendNextUpdate()
{
    if (d_updateQueue.empty()) {
        return;
    }
    bsl::shared_ptr<TopologyTransformer> topologyTransformer;
    do {
        topologyTransformer =
            bsl::make_shared<TopologyTransformer>(d_updateQueue.front().first);
        if (topologyTransformer->hasError()) {
            BALL_LOG_ERROR << "TopologyTransformer failed to update topology";
            close(rmqamqpt::Constants::CHANNEL_ERROR,
                  "Error occurred, while updating topology");

            d_updateQueue.front().second(rmqt::Result<>(
                "TopologyTransformer failed to update topology"));
            return;
        }
        if (topologyTransformer->isDone()) {
            // No topology being declared
            d_updateQueue.front().second(rmqt::Result<>());
            d_updateQueue.pop();
        }
    } while (topologyTransformer->isDone() && !d_updateQueue.empty());

    if (!d_updateQueue.empty()) {
        d_topologyTransformer =
            bsl::make_pair(topologyTransformer, d_updateQueue.front().second);
        while (topologyTransformer->hasNext()) {
            writeMessage(topologyTransformer->getNextMessage(),
                         &noopWriteHandler);
        }
    }
}

rmqt::Future<>
Channel::updateTopology(const rmqt::TopologyUpdate& topologyUpdate)
{
    rmqt::Future<>::Pair futurePair = rmqt::Future<>::make();
    d_updateQueue.push(bsl::make_pair(topologyUpdate, futurePair.first));
    if (!d_topologyTransformer.first || d_topologyTransformer.first->isDone()) {
        sendNextUpdate();
    }
    return futurePair.second;
}

void Channel::declareTopology()
{
    d_declareTopologyStartTime = bdlt::CurrentTime::now();
    bsl::shared_ptr<TopologyTransformer> topologyTransformer =
        bsl::make_shared<TopologyTransformer>(d_topology);

    if (topologyTransformer->hasError()) {
        BALL_LOG_ERROR
            << "TopologyTransformer failed to declare complete topology";
        close(rmqamqpt::Constants::CHANNEL_ERROR,
              "Error occurred, while declaring topology");
        d_topologyTransformer.first.reset();
        return;
    }
    if (topologyTransformer->isDone()) {
        // No topology being declared
        topologyDeclared(rmqt::Result<>());
        sendNextUpdate();
        return;
    }

    using bdlf::PlaceHolders::_1;
    TopologyUpdateConfirmCallback declareTopologyCallback =
        bdlf::BindUtil::bind(
            &Channel::topologyDeclaredCb, weak_from_this(), _1);
    d_topologyTransformer =
        bsl::make_pair(topologyTransformer, declareTopologyCallback);
    while (topologyTransformer->hasNext()) {
        writeMessage(topologyTransformer->getNextMessage(), DECLARING_TOPOLOGY);
    }
}

void Channel::topologyDeclaredCb(const bsl::weak_ptr<Channel>& weakSelf,
                                 const rmqt::Result<>& result)

{
    bsl::shared_ptr<Channel> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_TRACE << "Topology Declare Finished after channel destruction";
        // Closing
        return;
    }

    self->topologyDeclared(result);
}

void Channel::topologyDeclared(const rmqt::Result<>& result)
{
    d_metricPublisher->publishSummary(
        "topology_declare_time",
        (bdlt::CurrentTime::now() - d_declareTopologyStartTime)
            .totalSecondsAsDouble(),
        d_vhostTags);

    if (!result) {
        close(rmqamqpt::Constants::CHANNEL_ERROR,
              "Error occurred, while declaring topology");
        return;
    }

    updateState(TOPOLOGY_LOADED);

    d_hungProgressTimer->cancel();

    onOpen();
}

void Channel::processConfirmMethod(const rmqamqpt::ConfirmMethod& confirm)
{
    BALL_LOG_ERROR << "Received unexpected confirm method: " << confirm;
}

Channel::CleanupIndicator
Channel::processReceived(const rmqamqp::Message& message)
{
    if (d_state == CLOSED) {
        BALL_LOG_ERROR << "Received frame when channel is closed";
        return KEEP;
    }

    if (message.is<rmqamqpt::Method>()) {
        const rmqamqpt::Method& method = message.the<rmqamqpt::Method>();
        switch (method.classId()) {
            case rmqamqpt::ChannelMethod::CLASS_ID: {
                ChannelMethodProcessor channelMethodHandler(*this);

                return method.the<rmqamqpt::ChannelMethod>().apply(
                    channelMethodHandler);
                break; // GCC warning
            }
            case rmqamqpt::BasicMethod::CLASS_ID: {
                processBasicMethod(method.the<rmqamqpt::BasicMethod>());
                break;
            }
            case rmqamqpt::ConfirmMethod::CLASS_ID: {
                processConfirmMethod(method.the<rmqamqpt::ConfirmMethod>());
                break;
            }
            case rmqamqpt::QueueMethod::CLASS_ID: {
                processTopologyMethod(method);
                break;
            }
            case rmqamqpt::ExchangeMethod::CLASS_ID: {
                processTopologyMethod(method);
                break;
            }
            default: {
                BALL_LOG_ERROR << "UNHANDLED METHOD: " << method;
                break;
            }
        }
    }
    else if (message.is<rmqt::Message>()) {
        processMessage(message.the<rmqt::Message>());
    }
    else {
        BALL_LOG_ERROR << "Unable to process Message Type: "
                       << message.typeIndex();
    }

    return KEEP;
}

void Channel::processMessage(const rmqt::Message&)
{
    BALL_LOG_ERROR << "Throwing Content Away";
}

void Channel::onWriteComplete(const bsl::weak_ptr<Channel>& weakSelf)
{
    bsl::shared_ptr<Channel> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_TRACE
            << "Write completion handler called after channel destroyed";
        return;
    }

    if (self->d_state == CLOSED) {
        const bool startRetry = !self->d_permanentlyClosing;
        self->reset(startRetry);
    }
}

Channel::CleanupIndicator Channel::reset(bool startRetry)
{
    updateState(CLOSED);

    processFailures();

    onReset();
    if (startRetry) {
        d_retryHandler->retry(
            bdlf::BindUtil::bind(&Channel::retry, weak_from_this()));
    }
    d_metricPublisher->publishCounter(
        "channel_disconnected", 1, getVHostAndChannelTags());

    return KEEP;
}

void Channel::retry(const bsl::weak_ptr<Channel>& weakSelf)
{
    bsl::shared_ptr<Channel> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_TRACE << "Retry handler called during shutdown";
        return;
    }

    self->open();
}
void Channel::onReset() {}
void Channel::onFlowAllowed() {}
void Channel::onOpen() { ready(); }
void Channel::ready()
{
    BALL_LOG_TRACE << "Channel Ready";

    d_hungProgressTimer->cancel();
    d_state = READY;
    if (d_readyMade) {
        d_readyMade.value()(rmqt::Result<>());
        d_readyMade.reset();
    }
    d_metricPublisher->publishCounter(
        "channel_ready", 1, getVHostAndChannelTags());
}

void Channel::open()
{
    using bdlf::PlaceHolders::_1;
    d_hungProgressTimer->start(
        bdlf::BindUtil::bind(&Channel::channelHung, weak_from_this(), _1));

    writeMessage(Message(rmqamqpt::Method(
                     rmqamqpt::ChannelMethod(rmqamqpt::ChannelOpen()))),
                 CHANNEL_OPEN_SENT);
}

void Channel::writeMessage(const rmqamqp::Message& message, State newState)
{
    updateState(newState);
    BALL_LOG_TRACE << "State now set to: " << d_state;
    d_onAsyncWrite(
        bsl::make_shared<Message>(message),
        bdlf::BindUtil::bind(&Channel::onWriteComplete, weak_from_this()));
}

void Channel::writeMessage(
    const rmqamqp::Message& message,
    const rmqio::Connection::SuccessWriteCallback& callBack)
{
    d_onAsyncWrite(bsl::make_shared<Message>(message), callBack);
}

void Channel::gracefulClose()
{
    d_permanentlyClosing = true;
    close(rmqamqpt::Constants::REPLY_SUCCESS, "Closing Channel");
}

void Channel::close(rmqamqpt::Constants::AMQPReplyCode replyCode,
                    const bsl::string& replyText,
                    rmqamqpt::Constants::AMQPClassId failingClassId,
                    rmqamqpt::Constants::AMQPMethodId failingMethodId)
{
    writeMessage(
        Message(rmqamqpt::Method(rmqamqpt::ChannelMethod(rmqamqpt::ChannelClose(
            replyCode, replyText, failingClassId, failingMethodId)))),
        CHANNEL_CLOSE_SENT);
}

const bsl::string& Channel::vhostName() const { return d_vhostName; }

bsl::vector<bsl::pair<bsl::string, bsl::string> >
Channel::getVHostAndChannelTags()
{
    if (d_vhostAndChannelTags.empty()) {
        d_vhostAndChannelTags = d_vhostTags;
        d_vhostAndChannelTags.push_back(bsl::pair<bsl::string, bsl::string>(
            Metrics::CHANNELTYPE_TAG, channelType()));
    }

    return d_vhostAndChannelTags;
}

rmqt::Future<> Channel::waitForReady()
{
    if (d_state != Channel::READY) {
        rmqt::Future<>::Pair channelReadyFut = rmqt::Future<>::make();
        if (!d_readyMade) {
            d_readyMade = channelReadyFut.first;
        }
        else {
            rmqt::Future<>::Maker newMaker = channelReadyFut.first;
            rmqt::Future<>::Maker oldMaker = d_readyMade.value();

            using bdlf::PlaceHolders::_1;
            d_readyMade =
                bdlf::BindUtil::bind(&callBothMakers, oldMaker, newMaker, _1);
        }
        return channelReadyFut.second;
    }
    return rmqt::Future<>(rmqt::Result<>());
}

void Channel::updateState(State state)
{
    switch (state) {
        case READY:
            ready();
            break;
        case CLOSED:
            BALL_LOG_DEBUG << "Closing channel for vhost:" << d_vhostName;
            // fall through
        default:
            d_state = state;
    }
}

void Channel::channelHung(const bsl::weak_ptr<Channel>& weakSelf,
                          rmqio::Timer::InterruptReason reason)
{
    if (reason == rmqio::Timer::CANCEL) {
        return;
    }
    bsl::shared_ptr<Channel> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_TRACE << "Channel hung called after channel destruction";
        return;
    }

    BALL_LOG_ERROR << "Detected channel hung in setup. VHost: "
                   << self->vhostName()
                   << " Channel Type: " << self->channelType() << " in state "
                   << self->d_state << ". Triggering reconnection.";

    self->d_connErrorCb();
}

} // namespace rmqamqp
} // namespace BloombergLP
