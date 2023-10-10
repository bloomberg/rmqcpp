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

#include <rmqtestutil_mocktimerfactory.h>

#include <ball_log.h>
#include <bdlf_bind.h>

namespace BloombergLP {
namespace rmqtestutil {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQTESTUTIL.MOCKTIMERFACTORY")
}

MockTimerFactory::MockTimerFactory()
: d_now()
, d_timerCallbacks()
{
}

MockTimerFactory::~MockTimerFactory() { cancel(); }

void MockTimerFactory::cancel()
{
    // Invoke every timer with cancel as per boost spec
    for (bsl::list<TimerCallback>::iterator it = d_timerCallbacks.begin();
         it != d_timerCallbacks.end();
         ++it) {
        bsl::shared_ptr<MockTimer> lockedTimer = it->mockTimer.lock();

        // Break any possible timer ownership cycles
        if (lockedTimer) {
            lockedTimer->resetCallback(rmqio::Timer::Callback());
            lockedTimer->cancel();
        }
    }

    d_timerCallbacks.clear();
}

bsl::shared_ptr<rmqio::Timer>
MockTimerFactory::createWithTimeout(const bsls::TimeInterval& timeout)
{
    return bsl::make_shared<MockTimer>(shared_from_this(), timeout);
}

bsl::shared_ptr<rmqio::Timer>
MockTimerFactory::createWithCallback(const rmqio::Timer::Callback& callback)
{
    return bsl::make_shared<MockTimer>(shared_from_this(), callback);
}

void MockTimerFactory::step_time(const bsls::TimeInterval& timeout)
{
    d_now += timeout;

    bsl::list<TimerCallback>::iterator it = d_timerCallbacks.begin();

    if (it == d_timerCallbacks.end()) {
        BALL_LOG_TRACE << "No timers registered when step_time invoked";
    }

    while (it != d_timerCallbacks.end()) {
        BALL_LOG_TRACE << "Invoking timer which expires at " << it->expires_at;
        if (it->expires_at <= d_now) {
            bsl::shared_ptr<MockTimer> timer = it->mockTimer.lock();
            it                               = d_timerCallbacks.erase(it);
            if (timer) {
                timer->invoke();
            }
        }
        else {
            BALL_LOG_TRACE << "Not invoking timer which expires at "
                           << it->expires_at;
            ++it;
        }
    }
}

void MockTimerFactory::addCallback(const bsl::shared_ptr<MockTimer>& timer,
                                   bsls::TimeInterval expires_from_now)
{
    TimerCallback callback(timer, d_now + expires_from_now);
    d_timerCallbacks.push_front(callback);
}

bool MockTimerFactory::matchingTimer(const bsl::shared_ptr<MockTimer>& timer,
                                     const TimerCallback& callback)
{
    return callback.mockTimer.lock() == timer;
}

void MockTimerFactory::removeCallback(const bsl::shared_ptr<MockTimer>& timer)
{
    d_timerCallbacks.remove_if(bdlf::BindUtil::bind(
        &MockTimerFactory::matchingTimer, this, timer, bdlf::PlaceHolders::_1));
}

MockTimer::MockTimer(const bsl::shared_ptr<MockTimerFactory>& factory,
                     const bsls::TimeInterval& timeout)
: d_factory(factory)
, d_callback()
, d_timeout(timeout)
{
}

MockTimer::MockTimer(const bsl::shared_ptr<MockTimerFactory>& factory,
                     const rmqio::Timer::Callback& callback)
: d_factory(factory)
, d_callback(callback)
, d_timeout()
{
}

void MockTimer::reset(const bsls::TimeInterval& timeout)
{
    d_timeout = timeout;
    d_callback(Timer::CANCEL);
    d_factory->removeCallback(shared_from_this());
    d_factory->addCallback(shared_from_this(), d_timeout);
}

void MockTimer::resetCallback(const rmqio::Timer::Callback& callback)
{
    d_callback = callback;
}

void MockTimer::cancel()
{
    if (!d_callback)
        return;
    d_callback(Timer::CANCEL);
    d_factory->removeCallback(shared_from_this());
}

void MockTimer::start(const Timer::Callback& callback)
{
    d_callback = callback;
    d_factory->addCallback(shared_from_this(), d_timeout);
}

void MockTimer::invoke() { d_callback(Timer::EXPIRE); }

MockTimerFactory::TimerCallback::TimerCallback(
    const bsl::shared_ptr<MockTimer>& mockTimer,
    const bsls::TimeInterval& expiresAt)
: mockTimer(mockTimer)
, expires_at(expiresAt)
{
}
} // namespace rmqtestutil
} // namespace BloombergLP
