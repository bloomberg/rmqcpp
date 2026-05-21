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

#ifndef INCLUDED_RMQIO_CONNECTIONRETRYHANDLER
#define INCLUDED_RMQIO_CONNECTIONRETRYHANDLER

#include <rmqio_retryhandler.h>
#include <rmqio_timer.h>

#include <rmqt_result.h>

#include <bdlt_datetime.h>
#include <bsls_timeinterval.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_optional.h>

namespace BloombergLP {
namespace rmqio {

class ConnectionRetryHandler : public RetryHandler {
  public:
    /// Initialize retryHandler parameters
    /// \param timerFactory       Used to construct sleep timer
    /// \param errorCb            Error callback function, which will be called
    ///                         in case of channel or connection error
    /// \param retryStrategy    the retry strategy to use
    /// \param errorThreshold   threshold of time at which errorCb is called if
    ///                         there has been no success within
    ConnectionRetryHandler(const bsl::shared_ptr<TimerFactory>& timerFactory,
                           const rmqt::ErrorCallback& errorCb,
                           const rmqt::SuccessCallback& successCb,
                           const bsl::shared_ptr<RetryStrategy>& retryStrategy,
                           const bsls::TimeInterval& errorThreshold);

    virtual ~ConnectionRetryHandler() {}

    /// Trigger a retry attempt
    /// This will call the `onRetry` callback after the current backoff wait
    virtual void retry(const RetryHandler::RetryCallback& onRetry);

    /// Called as positive feedback
    virtual void success();

  private:
    void evaluateErrorThreshold();

  private:
    bsls::TimeInterval d_errorThreshold;
    bsl::optional<bsls::TimeInterval> d_errorSince;
};

} // namespace rmqio
} // namespace BloombergLP

#endif
