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

#include <rmqio_connectionretryhandler.h>

#include <rmqt_result.h>

#include <rmqtestutil_callcount.h>
#include <rmqtestutil_mockretrystrategy.t.h>
#include <rmqtestutil_mocktimerfactory.h>

#include <bdlf_bind.h>
#include <bdlt_currenttime.h>

#include <bsl_iostream.h>
#include <bslmt_threadutil.h>

#include <bsl_memory.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqio;
using namespace ::testing;
namespace {
template <int SECONDS, int NANO>
bsls::TimeInterval fixedTimeCb()
{
    return bsls::TimeInterval(SECONDS, NANO);
}

template <int ABSOLUTE_SECONDS, int ABSOLUTE_NANO>
void setTime()
{
    bdlt::CurrentTime::setCurrentTimeCallback(
        fixedTimeCb<ABSOLUTE_SECONDS, ABSOLUTE_NANO>);
}
} // namespace

class ConnectionRetryHandlerTests : public ::testing::Test {
  public:
    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
    bsl::shared_ptr<StrictMock<rmqtestutil::MockRetryStrategy> >
        d_retryStrategy;
    StrictMock<testing::MockFunction<void(const bsl::string&, int)> >
        d_onErrorCallback;
    StrictMock<testing::MockFunction<void()>> d_onSuccessCallback;
    rmqt::ErrorCallback d_onError;
    rmqt::SuccessCallback d_onSuccess;
    bdlt::CurrentTime::CurrentTimeCallback d_oldTimeCb;

    ConnectionRetryHandlerTests()
    : d_timerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    , d_retryStrategy(
          bsl::make_shared<StrictMock<rmqtestutil::MockRetryStrategy> >())
    , d_onError(bdlf::BindUtil::bind(
          &testing::MockFunction<void(const bsl::string&, int)>::Call,
          &d_onErrorCallback,
          bdlf::PlaceHolders::_1,
          bdlf::PlaceHolders::_2))
    , d_onSuccess(bdlf::BindUtil::bind(
        &testing::MockFunction<void()>::Call,
        &d_onSuccessCallback))
    , d_oldTimeCb(bdlt::CurrentTime::setCurrentTimeCallback(fixedTimeCb<0, 0>))
    {
    }

    ~ConnectionRetryHandlerTests()
    {
        bdlt::CurrentTime::setCurrentTimeCallback(d_oldTimeCb);
    }

    void retryExpectations(unsigned int waitTime = 0)
    {
        EXPECT_CALL(*d_retryStrategy, print(_)).WillRepeatedly(ReturnArg<0>());
        EXPECT_CALL(*d_retryStrategy, getNextRetryInterval())
            .WillOnce(Return(bsls::TimeInterval(waitTime)));
    }

    void attemptExpectations() { EXPECT_CALL(*d_retryStrategy, attempt()); }
};

TEST_F(ConnectionRetryHandlerTests, Breathing)
{
    ConnectionRetryHandler ConnectionRetryHandler(
        d_timerFactory, d_onError, d_onSuccess, d_retryStrategy, bsls::TimeInterval(2));
}

TEST_F(ConnectionRetryHandlerTests, RetryWithoutWait)
{
    ConnectionRetryHandler ConnectionRetryHandler(
        d_timerFactory, d_onError, d_onSuccess, d_retryStrategy, bsls::TimeInterval(2));
    int numRetries = 0;

    retryExpectations();
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(0));

    EXPECT_THAT(numRetries, Eq(1));
}

TEST_F(ConnectionRetryHandlerTests, MultipleRetry)
{

    ConnectionRetryHandler ConnectionRetryHandler(
        d_timerFactory,
        d_onError,
        d_onSuccess,
        d_retryStrategy,
        bsls::TimeInterval(2)); /* 3 */
    int numRetries = 0;

    retryExpectations();
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(0));

    retryExpectations();
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(0));

    EXPECT_THAT(numRetries, Eq(2));
}

TEST_F(ConnectionRetryHandlerTests, MultipleRetryWithWait)
{
    ConnectionRetryHandler ConnectionRetryHandler(
        d_timerFactory,
        d_onError,
        d_onSuccess,
        d_retryStrategy,
        bsls::TimeInterval(2)); /* 1,20 */
    int numRetries = 0;

    retryExpectations();
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(0));

    EXPECT_THAT(numRetries, Eq(1));

    retryExpectations(20);
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(20));

    EXPECT_THAT(numRetries, Eq(2));
}

TEST_F(ConnectionRetryHandlerTests, NoPrematureRetry)
{
    ConnectionRetryHandler ConnectionRetryHandler(
        d_timerFactory,
        d_onError,
        d_onSuccess,
        d_retryStrategy,
        bsls::TimeInterval(2)); /* 1,20 */
    int numRetries = 0;

    retryExpectations();
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));
    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(0));

    retryExpectations(20);
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

    d_timerFactory->step_time(bsls::TimeInterval(19));

    EXPECT_THAT(numRetries, Eq(1));

    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(1));

    EXPECT_THAT(numRetries, Eq(2));
}

TEST_F(ConnectionRetryHandlerTests, MultipleRetryWithWaitLimit)
{
    ConnectionRetryHandler ConnectionRetryHandler(
        d_timerFactory,
        d_onError,
        d_onSuccess,
        d_retryStrategy,
        bsls::TimeInterval(2)); /* 1,20, 21 */
    int numRetries = 0;

    retryExpectations();
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));
    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(0));

    EXPECT_THAT(numRetries, Eq(1));

    retryExpectations(20);
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));
    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(20));
    EXPECT_THAT(numRetries, Eq(2));

    retryExpectations(21);
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));
    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(21));
    EXPECT_THAT(numRetries, Eq(3));

    retryExpectations(54);
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));
    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(54));

    EXPECT_THAT(numRetries, Eq(4));
}

TEST_F(ConnectionRetryHandlerTests, RetryIsNotCalledAfterBeingDestroyed)
{
    int numRetries = 0;

    {
        ConnectionRetryHandler ConnectionRetryHandler(
            d_timerFactory, d_onError, d_onSuccess, d_retryStrategy, bsls::TimeInterval(2));

        retryExpectations();
        ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

        attemptExpectations();
        d_timerFactory->step_time(bsls::TimeInterval(0));

        EXPECT_THAT(numRetries, Eq(1));

        retryExpectations(30);
        ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));
    }
    d_timerFactory->step_time(bsls::TimeInterval(40));

    EXPECT_THAT(numRetries, Eq(1));
}

TEST_F(ConnectionRetryHandlerTests,
       ConnectionRetryHandlerCallsBackAfterThreshold)
{
    int numRetries = 0;

    {
        ConnectionRetryHandler ConnectionRetryHandler(
            d_timerFactory, d_onError, d_onSuccess, d_retryStrategy, bsls::TimeInterval(2));

        retryExpectations();
        ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

        attemptExpectations();
        d_timerFactory->step_time(bsls::TimeInterval(0));

        EXPECT_THAT(numRetries, Eq(1));

        setTime<2, 1>();

        EXPECT_CALL(
            d_onErrorCallback,
            Call(bsl::string("Unable to establish a connection with the broker "
                             "for 2s (more than threshold: 2s)"),
                 408));
        retryExpectations();
        ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));
    }
}

TEST_F(ConnectionRetryHandlerTests,
       ConnectionRetryHandlerDoesntCallsBackAfterSuccess)
{
    int numRetries = 0;

    ConnectionRetryHandler ConnectionRetryHandler(
        d_timerFactory, d_onError, d_onSuccess, d_retryStrategy, bsls::TimeInterval(2));

    retryExpectations();
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));

    attemptExpectations();
    d_timerFactory->step_time(bsls::TimeInterval(0));

    EXPECT_THAT(numRetries, Eq(1));

    EXPECT_CALL(*d_retryStrategy, success());
    ConnectionRetryHandler.success(); // first retry was successful, then we
                                      // disconnected and retried
    setTime<2, 1>();
    retryExpectations();
    ConnectionRetryHandler.retry(rmqtestutil::CallCount(&numRetries));
}
