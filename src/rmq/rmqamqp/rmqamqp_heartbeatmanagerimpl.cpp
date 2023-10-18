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

#include <rmqamqp_heartbeatmanagerimpl.h>

#include <rmqamqp_framer.h>

#include <rmqio_timer.h>

#include <ball_log.h>
#include <bdlf_bind.h>

#include <bsl_functional.h>
#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqamqp {
namespace {
/// Number of seconds per tick
const int TICK_TIME = 1;

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.HEARTBEATMANAGERIMPL")
} // namespace

HeartbeatManagerImpl::HeartbeatManagerImpl(
    const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory)
: d_timeoutSeconds()
, d_tickTimer(timerFactory->createWithCallback(
      bdlf::BindUtil::bind(&HeartbeatManagerImpl::handleTick,
                           this,
                           bdlf::PlaceHolders::_1)))
, d_sendHeartbeat()
, d_killConnection()
, d_active(false)
, d_ticksUntilDisconnect(0)
, d_ticksUntilHeartBeat(0)
, d_totalTicksUntilDisconnect(0)
, d_totalTicksUntilHeartBeat(0)
{
}

void HeartbeatManagerImpl::start(
    uint32_t timeoutSeconds,
    const HeartbeatManagerImpl::HeartbeatCallback& sendHeartbeat,
    const HeartbeatManagerImpl::ConnectionDeathCallback& onConnectionDeath)
{
    d_timeoutSeconds            = timeoutSeconds;
    d_sendHeartbeat             = sendHeartbeat;
    d_killConnection            = onConnectionDeath;
    d_active                    = true;
    d_totalTicksUntilDisconnect = d_timeoutSeconds * 2 / TICK_TIME + 1;
    d_ticksUntilDisconnect      = d_totalTicksUntilDisconnect;
    // Add one tick as we want to guarantee d_timeoutSeconds has passed since
    // the last update.
    // Double timeout for the first heartbeat as before 3.7.11 of RMQ broker
    // first hearbeat takes twice as long to arrive.
    // https://github.com/rabbitmq/rabbitmq-common/pull/293/files
    d_totalTicksUntilHeartBeat = d_timeoutSeconds / 2 / TICK_TIME;
    d_ticksUntilHeartBeat      = d_totalTicksUntilHeartBeat;

    startTickTimer();
}

void HeartbeatManagerImpl::stop()
{
    d_tickTimer->cancel();
    d_active = false;
}

void HeartbeatManagerImpl::handleTick(rmqio::Timer::InterruptReason reason)
{
    if (reason == rmqio::Timer::CANCEL) {
        // Cancelled
        return;
    }
    startTickTimer();
    --d_ticksUntilHeartBeat;
    --d_ticksUntilDisconnect;
    if (d_ticksUntilHeartBeat == 0) {
        BALL_LOG_DEBUG << "Heartbeat Triggered";
        d_sendHeartbeat(Framer::makeHeartbeatFrame());
        d_ticksUntilHeartBeat = d_totalTicksUntilHeartBeat;
    }
    if (d_ticksUntilDisconnect == 0) {
        BALL_LOG_ERROR << "Received no heartbeats for " << d_timeoutSeconds
                       << "seconds. Triggering connection termination";
        d_killConnection();
        d_ticksUntilDisconnect = d_totalTicksUntilDisconnect;
    }
}

void HeartbeatManagerImpl::startTickTimer()
{
    d_tickTimer->reset(bsls::TimeInterval(TICK_TIME));
}

void HeartbeatManagerImpl::notifyMessageSent()
{
    d_ticksUntilHeartBeat = d_totalTicksUntilHeartBeat;
}

void HeartbeatManagerImpl::notifyMessageReceived()
{
    d_ticksUntilDisconnect = d_totalTicksUntilDisconnect;
}

void HeartbeatManagerImpl::notifyHeartbeatReceived()
{
    d_totalTicksUntilDisconnect = d_timeoutSeconds / TICK_TIME + 1;
    d_ticksUntilDisconnect      = d_totalTicksUntilDisconnect;
}

} // namespace rmqamqp
} // namespace BloombergLP
