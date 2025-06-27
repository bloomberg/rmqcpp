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

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bdlt_currenttime.h>
#include <bdlt_datetimeinterval.h>
#include <bsls_timeinterval.h>

#include <bsl_limits.h>
#include <bsl_memory.h>
#include <bsl_optional.h>
#include <bsl_sstream.h>

namespace BloombergLP {
namespace rmqio {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQIO.CONNECTIONRETRYHANDLER")
} // namespace

ConnectionRetryHandler::ConnectionRetryHandler(
    const bsl::shared_ptr<TimerFactory>& timerFactory,
    const rmqt::ErrorCallback& errorCb,
    const rmqt::SuccessCallback& successCb,
    const bsl::shared_ptr<RetryStrategy>& retryStrategy,
    const bsls::TimeInterval& errorThreshold)
: RetryHandler(timerFactory, errorCb, successCb, retryStrategy)
, d_errorThreshold(errorThreshold)
, d_errorSince()

{
}

void ConnectionRetryHandler::retry(const RetryCallback& onRetry)
{
    RetryHandler::retry(onRetry);
    evaluateErrorThreshold();
}

void ConnectionRetryHandler::success()
{
    RetryHandler::success();
    d_errorSince.reset();
    successCallback()();
}

void ConnectionRetryHandler::evaluateErrorThreshold()
{
    if (!d_errorSince) {
        d_errorSince = bdlt::CurrentTime::now();
    }
    else {
        bsls::TimeInterval diff =
            bdlt::CurrentTime::now() - d_errorSince.value();
        if (diff > d_errorThreshold) {
            bsl::ostringstream errorMessage;
            errorMessage
                << "Unable to establish a connection with the broker for "
                << diff.totalSeconds()
                << "s (more than threshold: " << d_errorThreshold.totalSeconds()
                << "s)";
            errorCallback()(errorMessage.str(), rmqt::TIMEOUT);
        }
    }
}

} // namespace rmqio
} // namespace BloombergLP
