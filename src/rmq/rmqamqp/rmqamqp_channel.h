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

#ifndef INCLUDED_RMQAMQP_CHANNEL
#define INCLUDED_RMQAMQP_CHANNEL

#include <rmqamqp_message.h>
#include <rmqamqp_topologytransformer.h>
#include <rmqamqpt_basicmethod.h>
#include <rmqamqpt_queuemethod.h>

#include <rmqio_connection.h>
#include <rmqio_retryhandler.h>
#include <rmqp_metricpublisher.h>
#include <rmqt_future.h>
#include <rmqt_message.h>
#include <rmqt_topology.h>
#include <rmqt_topologyupdate.h>

#include <bsl_cstdint.h>
#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_queue.h>
#include <bsl_utility.h>
#include <bsl_vector.h>
#include <bslma_managedptr.h>
#include <bsls_timeinterval.h>

//@PURPOSE: Provide base Channel interface
//
//@CLASSES: Channel

namespace BloombergLP {
namespace rmqamqp {

class Channel : public bsl::enable_shared_from_this<Channel> {
  public:
    enum State {
        CHANNEL_OPEN_SENT,
        OPEN,
        DECLARING_TOPOLOGY,
        TOPOLOGY_LOADED,
        AWAITING_REPLY,
        READY,
        CHANNEL_CLOSE_SENT,
        CLOSED
    };

    enum CleanupIndicator { KEEP, CLEANUP };

    typedef bsl::function<void(const bsl::shared_ptr<rmqamqp::Message>&,
                               const rmqio::Connection::SuccessWriteCallback&)>
        AsyncWriteCallback;

    typedef bsl::function<void()> HungChannelCallback;

    typedef bsl::function<void(const rmqt::Result<>&)>
        TopologyUpdateConfirmCallback;

    static const int k_HUNG_CHANNEL_TIMER_SEC;

    virtual ~Channel() {}

    /// Begins the process of opening this channel
    virtual void open();

    /// Resets the channel and begin channel retry process if startRetry = true
    ///
    /// @return KEEP if this channel object should be kept around, and
    ///         open() called again when reconnected.
    /// @return CLEANUP if the channel was permanently shutting down as this
    ///         reset happened.
    virtual CleanupIndicator reset(bool startRetry = false);

    /// Begins the process of closing this channel
    virtual void close(rmqamqpt::Constants::AMQPReplyCode replyCode,
                       const bsl::string& replyText,
                       rmqamqpt::Constants::AMQPClassId failingClassId =
                           rmqamqpt::Constants::NO_CLASS,
                       rmqamqpt::Constants::AMQPMethodId failingMethodId =
                           rmqamqpt::Constants::NO_METHOD);

    /// Process a message intended for this channel id
    ///
    /// @return KEEP if this channel object should be kept around. This will be
    ///         the case most of the time.
    /// @return CLEANUP if this channel has now permanently closed, and can be
    ///         cleaned up. This will happen after the broker confirms a channel
    ///         closure with CLOSE-OK. Once CLEANUP is returned, the connection
    ///         can remove this channel & reuse it for another channel object.
    virtual CleanupIndicator processReceived(const rmqamqp::Message& message);

    /// Closes the channel permanently
    virtual void gracefulClose();

    virtual size_t inFlight() const   = 0;
    virtual size_t lifetimeId() const = 0;

    State state() const { return d_state; }

    rmqt::Future<> waitForReady();

    rmqt::Future<> updateTopology(const rmqt::TopologyUpdate& topologyUpdate);

    /// Return a string which summarises what this channel is
    /// For the purposes of identifying the channel for debug logs
    virtual bsl::string channelDebugName() const = 0;

  protected:
    bsl::string d_vhostName;
    bool d_flow;
    bsl::shared_ptr<rmqp::MetricPublisher> d_metricPublisher;
    bsl::vector<bsl::pair<bsl::string, bsl::string> > d_vhostTags;
    bsl::vector<bsl::pair<bsl::string, bsl::string> > d_vhostAndChannelTags;

    // Used to ensure we make forward progress during channel
    // open/topology&consumer declare
    bsl::shared_ptr<rmqio::Timer> d_hungProgressTimer;

    Channel(const rmqt::Topology& topology,
            const AsyncWriteCallback& onAsyncWrite,
            const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
            const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
            const bsl::string& vhost,
            const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
            const HungChannelCallback& connErrorCb);

    virtual void processBasicMethod(const rmqamqpt::BasicMethod&) = 0;
    virtual void processConfirmMethod(const rmqamqpt::ConfirmMethod&);
    virtual void processMessage(const rmqt::Message&);
    virtual void onOpen();
    virtual void onReset();
    virtual void onFlowAllowed();
    /// Process failures when client got disconnected from broker and try to
    /// reconnect.
    virtual void processFailures() = 0;

    void sendNextUpdate();

    void ready();

    // Callback for writes
    static void onWriteComplete(const bsl::weak_ptr<Channel>& weakSelf);

    void writeMessage(const rmqamqp::Message& message, State newState);

    void writeMessage(const rmqamqp::Message& message,
                      const rmqio::Connection::SuccessWriteCallback& callBack);

    /// Callback for re-opening channel
    static void retry(const bsl::weak_ptr<Channel>& weakSelf);

    const bsl::string& vhostName() const;

    virtual const char* channelType() const = 0;

    virtual bsl::vector<bsl::pair<bsl::string, bsl::string> >
    getVHostAndChannelTags();

    void updateState(State state);

  private:
    State d_state;
    rmqt::Topology d_topology;
    bsl::pair<bsl::shared_ptr<TopologyTransformer>,
              TopologyUpdateConfirmCallback>
        d_topologyTransformer;
    bsl::queue<bsl::pair<rmqt::TopologyUpdate, TopologyUpdateConfirmCallback> >
        d_updateQueue;

    AsyncWriteCallback d_onAsyncWrite;
    bsl::shared_ptr<rmqio::RetryHandler> d_retryHandler;
    bool d_permanentlyClosing;
    bsls::TimeInterval d_declareTopologyStartTime;
    bsl::optional<rmqt::Future<>::Maker> d_readyMade;
    HungChannelCallback d_connErrorCb;

    Channel(const Channel&) BSLS_KEYWORD_DELETED;
    Channel& operator=(const Channel&) BSLS_KEYWORD_DELETED;

    CleanupIndicator processChannelMethod(const rmqamqpt::ChannelMethod&);

    void processTopologyMethod(const rmqamqpt::Method&);

    void declareTopology();
    static void topologyDeclaredCb(const bsl::weak_ptr<Channel>& weakSelf,
                                   const rmqt::Result<>& result);
    void topologyDeclared(const rmqt::Result<>& result);

    class ChannelMethodProcessor;

    static void channelHung(const bsl::weak_ptr<Channel>& weakSelf,
                            rmqio::Timer::InterruptReason reason);

}; // class Channel

} // namespace rmqamqp
} // namespace BloombergLP

#endif
