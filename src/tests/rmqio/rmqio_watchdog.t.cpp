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

#include <rmqio_watchdog.h>

#include <rmqio_task.h>

#include <rmqtestutil_mocktimerfactory.h>

#include <bdlf_bind.h>
#include <bdlt_currenttime.h>
#include <bsls_timeinterval.h>

#include <bsl_memory.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqio;
using namespace ::testing;

class MockTask : public rmqio::Task {
  public:
    MOCK_METHOD0(run, void());
};

class WatchDogTests : public Test {
  public:
    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_mockTimerFactory;
    bdlt::CurrentTime::CurrentTimeCallback d_prev;
    static bsls::TimeInterval s_time;
    const int d_timeout;
    bsl::shared_ptr<MockTask> d_mockTask;
    bsl::shared_ptr<WatchDog> d_watchDog;

    WatchDogTests()
    : d_mockTimerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    , d_prev(bdlt::CurrentTime::setCurrentTimeCallback(time))
    , d_timeout(10)
    , d_mockTask(bsl::make_shared<MockTask>())
    , d_watchDog(bsl::make_shared<WatchDog>(bsls::TimeInterval(d_timeout)))
    {
        s_time = bsls::TimeInterval(0);
        d_watchDog->addTask(d_mockTask);
        d_watchDog->start(d_mockTimerFactory);
    }

    ~WatchDogTests() { bdlt::CurrentTime::setCurrentTimeCallback(d_prev); }

    static bsls::TimeInterval time();
};

bsls::TimeInterval WatchDogTests::s_time(0);

bsls::TimeInterval WatchDogTests::time() { return WatchDogTests::s_time; }

TEST_F(WatchDogTests, Breathing) {}

TEST_F(WatchDogTests, TaskIsRun)
{

    EXPECT_CALL(*d_mockTask, run());
    s_time += bsls::TimeInterval(d_timeout);
    d_mockTimerFactory->step_time(bsls::TimeInterval(d_timeout));
}

TEST_F(WatchDogTests, NoPrematureRun)
{
    EXPECT_CALL(*d_mockTask, run()).Times(0);
    s_time += bsls::TimeInterval(d_timeout - 1);
    d_mockTimerFactory->step_time(bsls::TimeInterval(d_timeout - 1));
}

TEST_F(WatchDogTests, TaskIsRunTwice)
{
    EXPECT_CALL(*d_mockTask, run());
    s_time += bsls::TimeInterval(d_timeout);
    d_mockTimerFactory->step_time(bsls::TimeInterval(d_timeout));

    Mock::VerifyAndClearExpectations(&d_mockTask);

    EXPECT_CALL(*d_mockTask, run());
    s_time += bsls::TimeInterval(d_timeout);
    d_mockTimerFactory->step_time(bsls::TimeInterval(d_timeout));
}
