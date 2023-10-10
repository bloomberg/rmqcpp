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

#ifndef INCLUDED_RMQTESTUTIL_MOCKEVENTLOOP_T
#define INCLUDED_RMQTESTUTIL_MOCKEVENTLOOP_T

#include <bsl_memory.h>
#include <rmqio_eventloop.h>
#include <rmqio_retryhandler.h>
#include <rmqtestutil_mocktimerfactory.h>

#include <bsl_functional.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace BloombergLP {
namespace rmqtestutil {

class MockEventLoop : public rmqio::EventLoop {
  public:
    MockEventLoop(
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& timerFactory =
            bsl::shared_ptr<rmqtestutil::MockTimerFactory>())
    : d_timerFactory(timerFactory)
    {
        ON_CALL(*this, postImpl(testing::_))
            .WillByDefault(testing::InvokeArgument<0>());

        if (d_timerFactory) {
            ON_CALL(*this, timerFactory())
                .WillByDefault(testing::Return(d_timerFactory));
        }
    }

    void setup() {}

    ~MockEventLoop() { join(); }
    MOCK_METHOD1(postImpl, void(const bsl::function<void()>&));
    MOCK_METHOD1(dispatchImpl, void(const bsl::function<void()>&));
    MOCK_METHOD0(onThreadStarted, void());
    MOCK_METHOD1(resolver, bsl::shared_ptr<rmqio::Resolver>(bool));
    MOCK_METHOD0(timerFactory, bsl::shared_ptr<rmqio::TimerFactory>());
    MOCK_CONST_METHOD0(isStarted, bool());
    MOCK_METHOD1(waitForEventLoopExit, bool(int64_t));

    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif
