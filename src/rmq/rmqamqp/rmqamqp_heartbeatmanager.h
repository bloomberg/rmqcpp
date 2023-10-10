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

#ifndef INCLUDED_RMQAMQP_HEARTBEATMANAGER
#define INCLUDED_RMQAMQP_HEARTBEATMANAGER

#include <bsl_cstdint.h>
#include <bsl_functional.h>

//@PURPOSE: Schedule AMQP heartbeat messages
//
//@CLASSES:
//  rmqamqp::HeartbeatManager: Own & manage the heartbeat generation schedule
//      and detect dead connections

namespace BloombergLP {
namespace rmqamqpt {
class Frame;
}
namespace rmqamqp {

class HeartbeatManager {
  public:
    // This interface owns and manages the timers required to detect heartbeat
    // timeouts and send required heartbeat messages to keep a connection alive
    // One heartbeat manager is required per connection

    HeartbeatManager();
    virtual ~HeartbeatManager();

    typedef bsl::function<void(const rmqamqpt::Frame&)> HeartbeatCallback;
    typedef bsl::function<void()> ConnectionDeathCallback;

    /// Start monitoring/triggering heartbeat callbacks
    /// Must only be called when stopped
    /// \param timeoutSeconds determines the heartbeat timeout value. If no
    ///      messages are received in this period the connection is considered
    ///      down
    /// \param onHeartbeat is called with a Heartbeat frame which must be sent
    ///      over the AMQP connection
    /// \param onConnectionDeath is called if/when notifyMessageReceived has
    /// not
    ///      been called once during one timeout period (timeoutSeconds)
    virtual void start(uint32_t timeoutSeconds,
                       const HeartbeatCallback& sendHeartbeat,
                       const ConnectionDeathCallback& onConnectionDeath) = 0;

    /// Stop monitoring for heartbeats and triggering callbacks
    /// Safe to call when started and stopped
    virtual void stop() = 0;

    /// Delays a heartbeat message being sent out until timeout/2 occurs
    /// without this method being called again
    /// Calls to this are ignored when start has not been called or is stopped
    virtual void notifyMessageSent() = 0;

    /// Delays the onConnectionDeath trigger being pulled
    /// Calls to this are ignored when start has not been called or is stopped
    virtual void notifyMessageReceived() = 0;

    /// Used to signal a heartbeat method was received. This does not replace a
    /// call to notifyMessageReceived as this does not push any timers back
    /// This exists solely to work around a bug in RabbitMQ < 3.7.11 where the
    /// first heartbeat takes twice as long to be sent.
    virtual void notifyHeartbeatReceived() = 0;

  private:
    HeartbeatManager(const HeartbeatManager&) BSLS_KEYWORD_DELETED;
    HeartbeatManager& operator=(const HeartbeatManager&) BSLS_KEYWORD_DELETED;
}; // class HeartbeatManager

} // namespace rmqamqp
} // namespace BloombergLP

#endif
