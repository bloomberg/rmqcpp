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

#include <rmqio_asioeventloop.h>

#include <gtest/gtest.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bslmt_condition.h>
#include <bslmt_lockguard.h>
#include <bslmt_mutex.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>

using namespace BloombergLP;
using namespace rmqio;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("TESTS.RMQIO.ASIOEVENTLOOP")
}

TEST(AsioEventLoop, Breathing)
{
    // Wrap in a scope to call loop destructor
    {
        AsioEventLoop loop;
    }
}

TEST(AsioEventLoop, startstop)
{
    // Wrap in a scope to call loop destructor
    AsioEventLoop loop;
    loop.start();
}

TEST(AsioEventLoop, GetState) {}

namespace {
void setBoolToTrue(bool& b) { b = true; }
} // namespace

TEST(AsioEventLoop, ExecuteTask)
{
    bool bp = false;

    // Wrapped in a scope to make the EventLoop desctructor run (to ensure the
    // IO thread has joined, thus ensuring that all work has been done)
    {
        AsioEventLoop loop;
        loop.post(bdlf::BindUtil::bind(&setBoolToTrue, bsl::ref(bp)));
        loop.start();
    }

    EXPECT_EQ(true, bp);
}

namespace {
void waitForCondition(bslmt::Mutex& mutex, bslmt::Condition& condition)
{
    bslmt::LockGuard<bslmt::Mutex> guard(&mutex);

    BALL_LOG_DEBUG << "Waiting on condition";
    condition.wait(&mutex);
}

void pushWaitForCondition(AsioEventLoop& eventLoop,
                          bslmt::Mutex& mutex,
                          bslmt::Condition& condition)
{
    // This extra hop is necessary because EventLoop::start asks the event loop
    // to do some useful work before returning

    eventLoop.post(bdlf::BindUtil::bind(
        &waitForCondition, bsl::ref(mutex), bsl::ref(condition)));
}
} // namespace

TEST(AsioEventLoop, waitForExitFailsWithWorkInProgress)
{
    AsioEventLoop loop;
    bslmt::Mutex mutex;
    bslmt::Condition condition;

    loop.start();

    loop.post(bdlf::BindUtil::bind(&pushWaitForCondition,
                                   bsl::ref(loop),
                                   bsl::ref(mutex),
                                   bsl::ref(condition)));

    EXPECT_FALSE(loop.waitForEventLoopExit(1));

    condition.broadcast();

    EXPECT_TRUE(loop.waitForEventLoopExit(1));
}
