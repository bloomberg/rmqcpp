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

#include <rmqio_backofflevelretrystrategy.h>

#include <rmqtestutil_callcount.h>
#include <rmqtestutil_mocktimerfactory.h>
#include <rmqtestutil_timeoverride.h>

#include <bdlf_bind.h>

#include <bsl_iostream.h>
#include <bslmt_threadutil.h>

#include <bsl_vector.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqio;
using namespace ::testing;

class MockCurrentTime {
  public:
    MOCK_CONST_METHOD0(currentTime, bsls::TimeInterval());

    bsls::TimeInterval operator()() const { return currentTime(); }
};

class BackoffLevelRetryStrategyTests
: public ::testing::TestWithParam<unsigned int> {
  protected:
    MockCurrentTime d_mockCurrentTime;
    unsigned int continuousRetries;
    unsigned int minWaitSeconds;
    unsigned int maxWaitSeconds;

  public:
    BackoffLevelRetryStrategyTests()
    : d_mockCurrentTime()
    , continuousRetries(3)
    , minWaitSeconds(1)
    , maxWaitSeconds(60)
    {
    }

    void firstTry(BackoffLevelRetryStrategy& retryStrategy)
    {
        retryStrategy.getNextRetryInterval();
        retryStrategy.attempt(); // first try
    }

    void simulateFailsUpToBackoffLevel(BackoffLevelRetryStrategy& retryStrategy,
                                       unsigned int level)
    {
        for (size_t i = 0; i < (level * continuousRetries); ++i) {
            retryStrategy.getNextRetryInterval();
            retryStrategy.attempt();
        }
    }

    bsl::vector<bsls::TimeInterval>
    getTimeIntervalsAtThisLevel(BackoffLevelRetryStrategy& retryStrategy)
    {
        bsl::vector<bsls::TimeInterval> result;
        for (size_t i = 0; i < (continuousRetries); ++i) {
            result.push_back(retryStrategy.getNextRetryInterval());
            retryStrategy.attempt();
        }
        return result;
    }
};

TEST_F(BackoffLevelRetryStrategyTests, Breathing)
{
    BackoffLevelRetryStrategy retryStrategyDefault;
}

TEST_F(BackoffLevelRetryStrategyTests, FirstTryWithoutWait)
{
    BackoffLevelRetryStrategy retryStrategy;
    EXPECT_THAT(retryStrategy.getNextRetryInterval(), Eq(bsls::TimeInterval()));
}

TEST_F(BackoffLevelRetryStrategyTests, MultipleContinuousRetry)
{
    BackoffLevelRetryStrategy retryStrategy(
        continuousRetries, minWaitSeconds, maxWaitSeconds);
    firstTry(retryStrategy);
    bsl::vector<bsls::TimeInterval> timeIntervals =
        getTimeIntervalsAtThisLevel(retryStrategy);
    for (size_t i = 0; i < timeIntervals.size(); ++i) {
        EXPECT_THAT(timeIntervals[i], Eq(bsls::TimeInterval()));
    }
}

TEST_F(BackoffLevelRetryStrategyTests, BackoffLevel1)
{
    BackoffLevelRetryStrategy retryStrategy(
        continuousRetries, minWaitSeconds, maxWaitSeconds);

    firstTry(retryStrategy);
    simulateFailsUpToBackoffLevel(retryStrategy, 1);

    bsl::vector<bsls::TimeInterval> timeIntervals =
        getTimeIntervalsAtThisLevel(retryStrategy);

    for (size_t i = 0; i < timeIntervals.size(); ++i) {
        EXPECT_THAT(timeIntervals[i], Eq(bsls::TimeInterval(minWaitSeconds)));
    }
}

TEST_F(BackoffLevelRetryStrategyTests, BackoffLevel2)
{
    BackoffLevelRetryStrategy retryStrategy(
        continuousRetries, minWaitSeconds, maxWaitSeconds);
    firstTry(retryStrategy);
    simulateFailsUpToBackoffLevel(retryStrategy, 2);

    bsl::vector<bsls::TimeInterval> timeIntervals =
        getTimeIntervalsAtThisLevel(retryStrategy);

    for (size_t i = 0; i < timeIntervals.size(); ++i) {
        if (i > 0) {
            EXPECT_THAT(timeIntervals[i], Gt(timeIntervals[i - 1]));
        }
        else {
            EXPECT_THAT(timeIntervals[i],
                        Gt(bsls::TimeInterval(minWaitSeconds)));
        }
    }
}
TEST_F(BackoffLevelRetryStrategyTests, BackoffLevel3)
{
    BackoffLevelRetryStrategy retryStrategy(
        continuousRetries, minWaitSeconds, maxWaitSeconds);
    firstTry(retryStrategy);
    simulateFailsUpToBackoffLevel(retryStrategy, 3);

    bsl::vector<bsls::TimeInterval> timeIntervals =
        getTimeIntervalsAtThisLevel(retryStrategy);

    for (size_t i = 0; i < timeIntervals.size(); ++i) {
        if (i > 0) {
            EXPECT_THAT(timeIntervals[i], Ge(timeIntervals[i - 1]));
        }
        else {
            EXPECT_THAT(timeIntervals[i],
                        Eq(bsls::TimeInterval(2 * (1 + continuousRetries) *
                                              minWaitSeconds)));
        }
        EXPECT_THAT(timeIntervals[i], Le(bsls::TimeInterval(maxWaitSeconds)));
    }
}

TEST_F(BackoffLevelRetryStrategyTests, BackoffLevel1ThenRecover)
{
    bsls::TimeInterval currTime = bsls::SystemTime::nowRealtimeClock();

    BackoffLevelRetryStrategy retryStrategy(
        continuousRetries,
        minWaitSeconds,
        maxWaitSeconds,
        bdlf::BindUtil::bind(&MockCurrentTime::currentTime,
                             &d_mockCurrentTime));

    retryStrategy.getNextRetryInterval();

    EXPECT_CALL(d_mockCurrentTime, currentTime())
        .WillOnce(Return(bsls::SystemTime::nowRealtimeClock()));
    retryStrategy.attempt(); // first try

    for (size_t i = 0; i <= (1 * continuousRetries); ++i) {
        EXPECT_CALL(d_mockCurrentTime, currentTime())
            .WillOnce(Return(currTime));

        retryStrategy.getNextRetryInterval();

        EXPECT_CALL(d_mockCurrentTime, currentTime())
            .WillOnce(Return(currTime.addSeconds(minWaitSeconds)));

        retryStrategy.attempt();
    }

    EXPECT_CALL(d_mockCurrentTime, currentTime())
        .WillOnce(
            Return(currTime.addSeconds(minWaitSeconds).addNanoseconds(1)));

    EXPECT_THAT(retryStrategy.getNextRetryInterval(),
                Eq(bsls::TimeInterval(0)));
}

TEST_F(BackoffLevelRetryStrategyTests, BackoffLevel2ThenRecover)
{
    bsls::TimeInterval currTime = bsls::SystemTime::nowRealtimeClock();

    BackoffLevelRetryStrategy retryStrategy(
        continuousRetries,
        minWaitSeconds,
        maxWaitSeconds,
        bdlf::BindUtil::bind(&MockCurrentTime::currentTime,
                             &d_mockCurrentTime));

    retryStrategy.getNextRetryInterval();

    EXPECT_CALL(d_mockCurrentTime, currentTime()).WillOnce(Return(currTime));
    retryStrategy.attempt(); // first try

    for (size_t i = 0; i <= (2 * continuousRetries); ++i) {
        EXPECT_CALL(d_mockCurrentTime, currentTime())
            .WillOnce(Return(currTime.addSeconds(minWaitSeconds)));

        retryStrategy.getNextRetryInterval();

        EXPECT_CALL(d_mockCurrentTime, currentTime())
            .WillOnce(Return(currTime.addSeconds(minWaitSeconds)));

        retryStrategy.attempt();
    }

    EXPECT_CALL(d_mockCurrentTime, currentTime())
        .WillOnce(
            Return(currTime.addSeconds(2 * minWaitSeconds).addNanoseconds(1)));

    EXPECT_THAT(retryStrategy.getNextRetryInterval(),
                Eq(bsls::TimeInterval(minWaitSeconds)));
}

TEST_F(BackoffLevelRetryStrategyTests, BackoffLevel3ThenRecover)
{
    bsls::TimeInterval currTime = bsls::SystemTime::nowRealtimeClock();

    BackoffLevelRetryStrategy retryStrategy(
        continuousRetries,
        minWaitSeconds,
        maxWaitSeconds,
        bdlf::BindUtil::bind(&MockCurrentTime::currentTime,
                             &d_mockCurrentTime));

    retryStrategy.getNextRetryInterval();

    EXPECT_CALL(d_mockCurrentTime, currentTime()).WillOnce(Return(currTime));
    retryStrategy.attempt(); // first try
    bsls::TimeInterval lastRetryInterval;
    for (size_t i = 0; i <= (3 * continuousRetries); ++i) {
        EXPECT_CALL(d_mockCurrentTime, currentTime())
            .WillOnce(Return(currTime.addSeconds(minWaitSeconds)));

        lastRetryInterval = retryStrategy.getNextRetryInterval();

        EXPECT_CALL(d_mockCurrentTime, currentTime())
            .WillOnce(Return(currTime.addSeconds(minWaitSeconds)));

        retryStrategy.attempt();
    }

    EXPECT_CALL(d_mockCurrentTime, currentTime())
        .WillOnce(Return(currTime.addSeconds(minWaitSeconds * continuousRetries)
                             .addNanoseconds(1)));

    EXPECT_THAT(retryStrategy.getNextRetryInterval(),
                Eq(bsls::TimeInterval(minWaitSeconds)));
}

TEST_F(BackoffLevelRetryStrategyTests, RecoverAfterALongTime)
{
    bsls::TimeInterval currTime = bsls::SystemTime::nowRealtimeClock();

    BackoffLevelRetryStrategy retryStrategy(
        continuousRetries,
        minWaitSeconds,
        maxWaitSeconds,
        bdlf::BindUtil::bind(&MockCurrentTime::currentTime,
                             &d_mockCurrentTime));

    retryStrategy.getNextRetryInterval();

    EXPECT_CALL(d_mockCurrentTime, currentTime()).WillOnce(Return(currTime));
    retryStrategy.attempt(); // first try
    for (size_t i = 0; i <= (50 * continuousRetries);
         ++i) { // lots of failures over a long time
        EXPECT_CALL(d_mockCurrentTime, currentTime())
            .WillOnce(Return(currTime.addSeconds(minWaitSeconds)));

        retryStrategy.getNextRetryInterval();

        EXPECT_CALL(d_mockCurrentTime, currentTime())
            .WillOnce(Return(currTime.addSeconds(minWaitSeconds)));
        retryStrategy.attempt();
    }

    EXPECT_CALL(d_mockCurrentTime, currentTime())
        .WillOnce(
            Return(currTime.addSeconds(continuousRetries * minWaitSeconds)
                       .addNanoseconds(
                           1))); // the last attempt connected for just over 60s

    // step back level 2
    EXPECT_THAT(
        retryStrategy.getNextRetryInterval(),
        Eq(bsls::TimeInterval(
            minWaitSeconds))); // only have to wait for the minimum again

    EXPECT_CALL(d_mockCurrentTime, currentTime()).WillOnce(Return(currTime));
    retryStrategy.attempt();

    EXPECT_CALL(d_mockCurrentTime, currentTime())
        .WillOnce(
            Return(currTime.addSeconds(minWaitSeconds).addNanoseconds(1)));

    // step back level 1
    EXPECT_THAT(retryStrategy.getNextRetryInterval(),
                Eq(bsls::TimeInterval(minWaitSeconds)));

    EXPECT_CALL(d_mockCurrentTime, currentTime()).WillOnce(Return(currTime));
    retryStrategy.attempt();

    EXPECT_CALL(d_mockCurrentTime, currentTime())
        .WillOnce(
            Return(currTime.addSeconds(minWaitSeconds).addNanoseconds(1)));
    // step back level 0
    EXPECT_THAT(retryStrategy.getNextRetryInterval(),
                Eq(bsls::TimeInterval(0)));
}
