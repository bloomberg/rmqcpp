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

#include <rmqio_eventloop.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bdlf_bind.h>

#include <bsl_functional.h>
#include <bsl_memory.h>

using namespace BloombergLP;
using namespace rmqio;
using namespace ::testing;

namespace {

int someFunction() { return 0x1337; }

class ConcreteEventLoop : public EventLoop {
    // although this looks similar to MockEvent Loop, the key difference is that
    // it only Mocks Pure Virtual Functions of the EventLoop interface
  public:
    ~ConcreteEventLoop() { join(); }

    MOCK_METHOD1(postImpl, void(const bsl::function<void()>&));
    MOCK_METHOD1(dispatchImpl, void(const bsl::function<void()>&));
    MOCK_METHOD0(onThreadStarted, void());
    MOCK_METHOD1(resolver, bsl::shared_ptr<rmqio::Resolver>(bool));
    MOCK_METHOD0(timerFactory, bsl::shared_ptr<rmqio::TimerFactory>());
    MOCK_METHOD1(waitForEventLoopExit, bool(int64_t));
};

} // namespace

TEST(EventLoop, Breathing)
{
    // Wrap in a scope to call loop destructor
    {
        ConcreteEventLoop loop;
    }
}

TEST(EventLoop, startstop)
{
    ConcreteEventLoop loop;
    EXPECT_CALL(loop, postImpl(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(loop, onThreadStarted()).Times(1);
    loop.start();
    EXPECT_TRUE(loop.isStarted());
}

TEST(EventLoop, PostF)
{
    ConcreteEventLoop loop;
    EXPECT_CALL(loop, postImpl(_)).WillOnce(InvokeArgument<0>());
    rmqt::Future<int> future =
        loop.postF<int>(bdlf::BindUtil::bind(&someFunction));
    EXPECT_TRUE(future.blockResult());
    EXPECT_THAT(*future.blockResult().value(), Eq(someFunction()));
}
