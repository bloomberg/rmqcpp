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

#ifndef INCLUDED_RMQIO_RETRYHANDLER
#define INCLUDED_RMQIO_RETRYHANDLER

#include <rmqio_retrystrategy.h>
#include <rmqio_timer.h>

#include <rmqt_result.h>

#include <bdlt_datetime.h>
#include <bsls_keyword.h>
#include <bsls_timeinterval.h>

#include <bsl_functional.h>
#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqio {

class RetryHandler {
  public:
    typedef bsl::function<void()> RetryCallback;

    /// Initialize retryHandler parameters
    /// \param timerFactory       Used to construct sleep timer
    /// \param errorCb            Error callback function, which will be called
    ///                         in case of channel or connection error
    /// \param retryStrategy    the retry strategy to use
    RetryHandler(const bsl::shared_ptr<TimerFactory>& timerFactory,
                 const rmqt::ErrorCallback& errorCb,
                 const rmqt::SuccessCallback& successCb,
                 const bsl::shared_ptr<RetryStrategy>& retryStrategy);

    virtual ~RetryHandler() {}

    /// Trigger a retry attempt
    /// This will call the `onRetry` callback after the current backoff wait
    virtual void retry(const RetryCallback& onRetry);

    /// Called as positive feedback
    virtual void success();

    virtual const rmqt::ErrorCallback& errorCallback() const
    {
        return d_onError;
    }

    virtual const rmqt::SuccessCallback& successCallback() const
    {
      return d_onSuccess;
    }

  private:
    RetryHandler(const RetryHandler&) BSLS_KEYWORD_DELETED;
    RetryHandler& operator=(const RetryHandler&) BSLS_KEYWORD_DELETED;

    void handleRetry(Timer::InterruptReason reason);
    void scheduleRetry();

  private:
    bsl::shared_ptr<Timer> d_sleepTimer;
    bsl::shared_ptr<RetryStrategy> d_retryStrategy;
    RetryCallback d_retryCallback;
    rmqt::ErrorCallback d_onError;
    rmqt::SuccessCallback d_onSuccess;
};

} // namespace rmqio
} // namespace BloombergLP

#endif
