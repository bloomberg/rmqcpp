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

#ifndef INCLUDED_RMQAMQP_HEARTBEATMANAGERIMPL
#define INCLUDED_RMQAMQP_HEARTBEATMANAGERIMPL

#include <rmqamqp_framer.h>
#include <rmqamqp_heartbeatmanager.h>

#include <rmqio_timer.h>

#include <ball_log.h>
#include <bdlf_bind.h>

#include <bsl_functional.h>
#include <bsl_memory.h>

//@PURPOSE: Schedule AMQP heartbeat messsages
//
//@CLASSES:
//  rmqamqp::HeartbeatManagerImpl: Implements rmqamqp::HeartbeatManager

namespace BloombergLP {
namespace rmqamqpt {
class Frame;
}
namespace rmqamqp {

class HeartbeatManagerImpl : public HeartbeatManager {
  public:
    typedef bsl::function<void(const rmqamqpt::Frame&)> HeartbeatCallback;
    typedef bsl::function<void()> ConnectionDeathCallback;

    /// Construct a HeartbeatManager associated with the timer factory.
    /// This class will do nothing until start() is called
    /// \param timerFactory factory for constructing timers
    HeartbeatManagerImpl(
        const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory);

    ~HeartbeatManagerImpl() {}

    virtual void start(uint32_t timeoutSeconds,
                       const HeartbeatCallback& sendHeartbeat,
                       const ConnectionDeathCallback& onConnectionDeath)
        BSLS_KEYWORD_OVERRIDE;
    virtual void stop() BSLS_KEYWORD_OVERRIDE;

    virtual void notifyMessageSent() BSLS_KEYWORD_OVERRIDE;
    virtual void notifyMessageReceived() BSLS_KEYWORD_OVERRIDE;

    virtual void notifyHeartbeatReceived() BSLS_KEYWORD_OVERRIDE;

  private:
    HeartbeatManagerImpl(const HeartbeatManagerImpl&) BSLS_KEYWORD_DELETED;

    HeartbeatManagerImpl&
    operator=(const HeartbeatManagerImpl&) BSLS_KEYWORD_DELETED;
    void handleTick(rmqio::Timer::InterruptReason reason);

    void startTickTimer();

  private:
    uint32_t d_timeoutSeconds;

    bsl::shared_ptr<rmqio::Timer> d_tickTimer;
    HeartbeatCallback d_sendHeartbeat;
    ConnectionDeathCallback d_killConnection;
    bool d_active;
    size_t d_ticksUntilDisconnect;
    size_t d_ticksUntilHeartBeat;
    size_t d_totalTicksUntilDisconnect;
    size_t d_totalTicksUntilHeartBeat;
}; // class HeartbeatManagerImpl

} // namespace rmqamqp
} // namespace BloombergLP

#endif
