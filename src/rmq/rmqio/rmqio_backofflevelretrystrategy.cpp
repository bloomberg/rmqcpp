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

#include <ball_log.h>

#include <bsl_algorithm.h>
#include <bsl_limits.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqio {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("BASICRETRYSTRATEGY")
}

BackoffLevelRetryStrategy::BackoffLevelRetryStrategy(
    unsigned int continuousRetries /* = 5 */,
    unsigned int minWaitSeconds /* = 1 */,
    unsigned int maxWaitSeconds /* = 60 */,
    const CurrentTime& currentTime /* = &bsls::SystemTime::nowRealtimeClock */)
: d_backoffLevel(IMMEDIATE)
, d_continuousRetries(continuousRetries)
, d_minWaitSeconds(minWaitSeconds)
, d_maxWaitSeconds(maxWaitSeconds)
, d_currentTime(currentTime)
, d_numRetries()
, d_currentWait()
, d_lastRetry()
{
}

void BackoffLevelRetryStrategy::attempt()
{
    if (d_lastRetry.totalNanoseconds() &&
        d_numRetries < bsl::numeric_limits<unsigned int>::max()) {
        ++d_numRetries;
    }
    d_lastRetry = d_currentTime();
}

void BackoffLevelRetryStrategy::success()
{
    d_currentWait.setTotalSeconds(0);
    moveCircuitBreaker(false);
}

void BackoffLevelRetryStrategy::fail()
{
    if (d_numRetries >= d_continuousRetries) {
        moveCircuitBreaker(true);
    }
}

bsls::TimeInterval BackoffLevelRetryStrategy::getNextRetryInterval()
{
    if (d_lastRetry.totalNanoseconds()) {
        bsls::TimeInterval thresholdRetryTime = d_lastRetry;
        thresholdRetryTime.addSeconds(bsl::max<unsigned int>(
            d_minWaitSeconds,
            bsl::min<unsigned int>((d_minWaitSeconds * d_continuousRetries),
                                   d_currentWait.seconds())));
        (d_currentTime() > thresholdRetryTime) ? success() : fail();
        calculateNextInterval();
    }
    return d_currentWait;
}

bsl::ostream& BackoffLevelRetryStrategy::print(bsl::ostream& os) const
{
    return os << "state: " << d_backoffLevel << ", retries: " << d_numRetries
              << ", currentWait: " << d_currentWait;
}

void BackoffLevelRetryStrategy::moveCircuitBreaker(bool toward_exponential)
{
    int next_state = d_backoffLevel + (toward_exponential ? 1 : -1);
    if (next_state < IMMEDIATE)
        next_state = IMMEDIATE;
    if (next_state > EXPONENTIAL)
        next_state = EXPONENTIAL;
    d_backoffLevel = static_cast<BackoffLevel>(next_state);
    d_numRetries   = 0;
}

void BackoffLevelRetryStrategy::calculateNextInterval()
{
    switch (d_backoffLevel) {
        case IMMEDIATE:
            break;
        case CONSTANT:
            d_currentWait.setTotalSeconds(d_minWaitSeconds);
            break;
        case LINEAR:
            d_currentWait.addSeconds(d_minWaitSeconds);
            break;
        case EXPONENTIAL: {
            d_currentWait += d_currentWait;
        } break;
    }

    if (d_currentWait.seconds() > d_maxWaitSeconds) {
        d_currentWait.setTotalSeconds(d_maxWaitSeconds);
    }
}

} // namespace rmqio
} // namespace BloombergLP
