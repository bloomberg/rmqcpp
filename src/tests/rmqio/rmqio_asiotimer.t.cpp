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

#include <rmqio_asiotimer.h>

#include <rmqio_asioeventloop.h>

#include <rmqtestutil_timeoverride.h>

#include <bdlf_bind.h>
#include <boost/asio.hpp>
#include <bsls_timeinterval.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_memory.h>

using namespace BloombergLP;
using namespace rmqio;
using namespace ::testing;

namespace {
typedef rmqio::basic_AsioTimer<boost::asio::deadline_timer::time_type,
                               rmqtestutil::TimeOverride>
    FakeAsioTimer;
} // namespace

class MockCallback {
  public:
    MOCK_METHOD1(callback, void(Timer::InterruptReason));
};

class AsioTimerTests : public Test {
  public:
    MockCallback d_mockCallback;
    Timer::Callback d_callback;
    AsioEventLoop d_io;
    bsl::shared_ptr<FakeAsioTimer> d_timer;

    AsioTimerTests()
    : d_mockCallback()
    , d_callback(bdlf::BindUtil::bind(&MockCallback::callback,
                                      &d_mockCallback,
                                      bdlf::PlaceHolders::_1))
    , d_io()
    , d_timer(bsl::make_shared<FakeAsioTimer>(bsl::ref(d_io.context()),
                                              bsls::TimeInterval(10)))
    {
    }
};

TEST_F(AsioTimerTests, BreathingTest)
{
    // Timer constructed in fixture
}

TEST_F(AsioTimerTests, CallbackWhenExpires)
{
    EXPECT_CALL(d_mockCallback, callback(Timer::EXPIRE)).Times(1);
    d_timer->start(d_callback);
    rmqtestutil::TimeOverride::step_time(boost::posix_time::seconds(10));
    EXPECT_THAT(d_io.context().run_one(), Eq(1));
}

TEST_F(AsioTimerTests, Cancel)
{
    EXPECT_CALL(d_mockCallback, callback(Timer::CANCEL)).Times(1);
    d_timer->start(d_callback);
    d_timer->cancel();
    EXPECT_THAT(d_io.context().poll_one(), Eq(1));
}

TEST_F(AsioTimerTests, Reset)
{
    {
        InSequence s;
        EXPECT_CALL(d_mockCallback, callback(Timer::CANCEL)).Times(1);
        EXPECT_CALL(d_mockCallback, callback(Timer::EXPIRE)).Times(1);
    }
    d_timer->start(d_callback);
    d_timer->reset(bsls::TimeInterval(10));
    rmqtestutil::TimeOverride::step_time(boost::posix_time::seconds(10));
    EXPECT_THAT(d_io.context().run_one(), Eq(1));
    EXPECT_THAT(d_io.context().run_one(), Eq(1));
}

TEST_F(AsioTimerTests, ResetCallsCancelImmediately)
{
    EXPECT_CALL(d_mockCallback, callback(Timer::CANCEL)).Times(1);
    d_timer->start(d_callback);
    d_timer->reset(bsls::TimeInterval(10));
    EXPECT_THAT(d_io.context().poll_one(), Eq(1));
}

TEST_F(AsioTimerTests, CancelIsSilentlyIgnoredOnDestruction)
{
    {
        bsl::shared_ptr<FakeAsioTimer> timer = bsl::make_shared<FakeAsioTimer>(
            bsl::ref(d_io.context()), bsls::TimeInterval(10));
        timer->start(d_callback);
    }
    EXPECT_CALL(d_mockCallback, callback(Timer::CANCEL)).Times(0);
    EXPECT_THAT(d_io.context().run_one(), Eq(1));
}
