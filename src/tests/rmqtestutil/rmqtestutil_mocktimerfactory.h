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

#ifndef INCLUDED_RMQTESTUTIL_MOCKTIMERFACTORY_H
#define INCLUDED_RMQTESTUTIL_MOCKTIMERFACTORY_H

#include <rmqio_timer.h>

#include <bsl_list.h>
#include <bsl_memory.h>
#include <bsls_keyword.h>
#include <bsls_timeinterval.h>

namespace BloombergLP {
namespace rmqtestutil {

class MockTimerFactory;

class MockTimer : public rmqio::Timer,
                  public bsl::enable_shared_from_this<MockTimer> {

  public:
    MockTimer(const bsl::shared_ptr<MockTimerFactory>& factory,
              const bsls::TimeInterval& timeout);
    MockTimer(const bsl::shared_ptr<MockTimerFactory>& factory,
              const rmqio::Timer::Callback& callback);
    void cancel() BSLS_KEYWORD_OVERRIDE;
    void
    resetCallback(const rmqio::Timer::Callback& callback) BSLS_KEYWORD_OVERRIDE;
    void start(const Callback& callback) BSLS_KEYWORD_OVERRIDE;
    void reset(const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;
    void invoke();

  private:
    const bsl::shared_ptr<MockTimerFactory> d_factory;
    Timer::Callback d_callback;
    bsls::TimeInterval d_timeout;
};

class MockTimerFactory : public rmqio::TimerFactory,
                         public bsl::enable_shared_from_this<MockTimerFactory> {
    friend class MockTimer;

  public:
    MockTimerFactory();
    ~MockTimerFactory();

    void cancel();

    void step_time(const bsls::TimeInterval& timeout);

    bsl::shared_ptr<rmqio::Timer>
    createWithTimeout(const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;

    bsl::shared_ptr<rmqio::Timer> createWithCallback(
        const rmqio::Timer::Callback& callback) BSLS_KEYWORD_OVERRIDE;

  private:
    class TimerCallback {
      public:
        TimerCallback(const bsl::shared_ptr<MockTimer>& mockTimer,
                      const bsls::TimeInterval& expiresAt);
        const bsl::weak_ptr<MockTimer> mockTimer;
        const bsls::TimeInterval expires_at;
    };
    void addCallback(const bsl::shared_ptr<MockTimer>& timer,
                     bsls::TimeInterval expires_at);
    void removeCallback(const bsl::shared_ptr<MockTimer>& timer);
    bool matchingTimer(const bsl::shared_ptr<MockTimer>& timer,
                       const TimerCallback& callback);

    bsls::TimeInterval d_now;
    bsl::list<TimerCallback> d_timerCallbacks;
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif // INCLUDED_RMQTESTUTIL_MOCKTIMERFACTORY_H
