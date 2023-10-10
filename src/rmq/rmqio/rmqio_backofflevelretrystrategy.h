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

#ifndef INCLUDED_RMQIO_BACKOFFLEVELRETRYSTRATEGY
#define INCLUDED_RMQIO_BACKOFFLEVELRETRYSTRATEGY

#include <rmqio_retrystrategy.h>

#include <bsls_keyword.h>
#include <bsls_systemtime.h>
#include <bsls_timeinterval.h>

#include <bsl_functional.h>
#include <bsl_iosfwd.h>
#include <bsl_list.h>
#include <bsl_utility.h>

namespace BloombergLP {
namespace rmqio {

class BackoffLevelRetryStrategy : public RetryStrategy {
  public:
    typedef bsl::function<bsls::TimeInterval()> CurrentTime;

    /// \param continuousRetries Number of continuous connection retries
    ///                          without sleeping
    /// \param minWaitSeconds    Min Seconds to sleep before
    ///                          next connection retry
    /// \param maxWaitSeconds    Maximum seconds to sleep
    ///                          during any retry
    BackoffLevelRetryStrategy(
        unsigned int continuousRetries = 5,
        unsigned int minWaitSeconds    = 1,
        unsigned int maxWaitSeconds    = 60,
        const CurrentTime&             = &bsls::SystemTime::nowRealtimeClock);

    /// Should be called when a retry is attempted
    virtual void attempt() BSLS_KEYWORD_OVERRIDE;

    /// Can be called when an attempt has been successful
    virtual void success() BSLS_KEYWORD_OVERRIDE;

    /// \return a relative time period for when to make the next attempt
    virtual bsls::TimeInterval getNextRetryInterval() BSLS_KEYWORD_OVERRIDE;

    /// Should print out the current state of the strategy
    virtual bsl::ostream& print(bsl::ostream& os) const BSLS_KEYWORD_OVERRIDE;

    ~BackoffLevelRetryStrategy() {}

  private:
    enum BackoffLevel {
        IMMEDIATE = 0,
        CONSTANT,
        LINEAR,
        EXPONENTIAL
    } d_backoffLevel;

    void fail();

    void moveCircuitBreaker(bool toward_exponential);

    void calculateNextInterval();

    const unsigned int d_continuousRetries;
    const unsigned int d_minWaitSeconds;
    const unsigned int d_maxWaitSeconds;
    const CurrentTime d_currentTime;
    unsigned int d_numRetries;
    bsls::TimeInterval d_currentWait;
    bsls::TimeInterval d_lastRetry;
};

} // namespace rmqio
} // namespace BloombergLP

#endif
