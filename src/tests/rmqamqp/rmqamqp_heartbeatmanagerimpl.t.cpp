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

#include <rmqtestutil_callcount.h>
#include <rmqtestutil_mocktimerfactory.h>

#include <bsl_memory.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

class HeartbeatManager : public Test {
  public:
    HeartbeatManager()
    : d_heartbeatCallCount(0)
    , d_connectionKilledCount(0)
    , d_timerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    {
    }

    int d_heartbeatCallCount;
    int d_connectionKilledCount;

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;

    void tick(size_t seconds)
    {
        for (size_t i = 0; i < seconds; ++i) {
            d_timerFactory->step_time(bsls::TimeInterval(1));
        }
    }
};

TEST_F(HeartbeatManager, Construct)
{
    rmqamqp::HeartbeatManagerImpl hbManager(d_timerFactory);
}

TEST_F(HeartbeatManager, HeartbeatSendTriggersFirst)
{
    const uint32_t TIMEOUT_SEC = 4;

    rmqamqp::HeartbeatManagerImpl hbManager(d_timerFactory);
    hbManager.start(TIMEOUT_SEC,
                    rmqtestutil::CallCount(&d_heartbeatCallCount),
                    rmqtestutil::CallCount(&d_connectionKilledCount));

    // Move time 2 seconds into the future
    tick(2);

    EXPECT_THAT(d_heartbeatCallCount, Eq(1));
    EXPECT_THAT(d_connectionKilledCount, Eq(0));
}

TEST_F(HeartbeatManager, StopCancelsTimers)
{
    const uint32_t TIMEOUT_SEC = 4;

    rmqamqp::HeartbeatManagerImpl hbManager(d_timerFactory);
    hbManager.start(TIMEOUT_SEC,
                    rmqtestutil::CallCount(&d_heartbeatCallCount),
                    rmqtestutil::CallCount(&d_connectionKilledCount));
    hbManager.stop();

    tick(8);

    // No timers triggered
    EXPECT_THAT(d_heartbeatCallCount, Eq(0));
    EXPECT_THAT(d_connectionKilledCount, Eq(0));
}

TEST_F(HeartbeatManager, DisconnectTriggersLateForFirstHeartbeat)
{
    const uint32_t TIMEOUT_SEC = 4;

    rmqamqp::HeartbeatManagerImpl hbManager(d_timerFactory);
    hbManager.start(TIMEOUT_SEC,
                    rmqtestutil::CallCount(&d_heartbeatCallCount),
                    rmqtestutil::CallCount(&d_connectionKilledCount));

    // Trigger < 3.7.11 behaviour (disconnect after timeout * 2)

    tick(8);
    EXPECT_THAT(d_heartbeatCallCount, Eq(4));
    EXPECT_THAT(d_connectionKilledCount, Eq(0));

    tick(1);
    EXPECT_THAT(d_heartbeatCallCount, Eq(4));
    EXPECT_THAT(d_connectionKilledCount, Eq(1));
}

TEST_F(HeartbeatManager, DisconnectTriggersAfterFirstHeartbeat)
{
    const uint32_t TIMEOUT_SEC = 4;

    rmqamqp::HeartbeatManagerImpl hbManager(d_timerFactory);
    hbManager.start(TIMEOUT_SEC,
                    rmqtestutil::CallCount(&d_heartbeatCallCount),
                    rmqtestutil::CallCount(&d_connectionKilledCount));
    hbManager.notifyHeartbeatReceived();

    // Trigger 3.7.11+ behaviour (disconnect after timeout)
    tick(4);

    EXPECT_THAT(d_heartbeatCallCount, Eq(2));
    EXPECT_THAT(d_connectionKilledCount, Eq(0));

    tick(1);

    EXPECT_THAT(d_heartbeatCallCount, Eq(2));
    EXPECT_THAT(d_connectionKilledCount, Eq(1));
}

TEST_F(HeartbeatManager, NotifySendStopsSendHeartbeat)
{
    const uint32_t TIMEOUT_SEC = 4;

    rmqamqp::HeartbeatManagerImpl hbManager(d_timerFactory);
    // Trigger 3.7.11+ behaviour (disconnect after timeout*2)
    hbManager.start(TIMEOUT_SEC,
                    rmqtestutil::CallCount(&d_heartbeatCallCount),
                    rmqtestutil::CallCount(&d_connectionKilledCount));
    hbManager.notifyHeartbeatReceived();

    tick(1);
    hbManager.notifyMessageSent();

    tick(1);
    hbManager.notifyMessageSent();

    // Advance to kill timeout
    tick(1);
    hbManager.notifyMessageSent();
    tick(1);
    hbManager.notifyMessageSent();
    // Ensure no callbacks were hit yet
    EXPECT_THAT(d_heartbeatCallCount, Eq(0));
    EXPECT_THAT(d_connectionKilledCount, Eq(0));
    tick(1);
    EXPECT_THAT(d_heartbeatCallCount, Eq(0));
    EXPECT_THAT(d_connectionKilledCount, Eq(1));
}

TEST_F(HeartbeatManager, NotifyRecieveStopsDisconnect)
{
    const uint32_t TIMEOUT_SEC = 4;

    rmqamqp::HeartbeatManagerImpl hbManager(d_timerFactory);
    hbManager.start(TIMEOUT_SEC,
                    rmqtestutil::CallCount(&d_heartbeatCallCount),
                    rmqtestutil::CallCount(&d_connectionKilledCount));

    tick(2);

    EXPECT_THAT(d_heartbeatCallCount, Eq(1));

    hbManager.notifyMessageReceived();
    tick(2);

    EXPECT_THAT(d_heartbeatCallCount, Eq(2));

    hbManager.notifyMessageReceived();
    tick(2);

    EXPECT_THAT(d_heartbeatCallCount, Eq(3));
    EXPECT_THAT(d_connectionKilledCount, Eq(0));
}
