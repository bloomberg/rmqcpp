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

#include <rmqio_retryhandler.h>

#include <rmqt_result.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bdlt_datetimeinterval.h>

#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqio {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQIO.RETRYHANDLER")
} // namespace

RetryHandler::RetryHandler(const bsl::shared_ptr<TimerFactory>& timerFactory,
                           const rmqt::ErrorCallback& errorCb,
                           const rmqt::SuccessCallback& successCb,
                           const bsl::shared_ptr<RetryStrategy>& retryStrategy)
: d_sleepTimer(timerFactory->createWithCallback(
      bdlf::BindUtil::bind(&RetryHandler::handleRetry,
                           this,
                           bdlf::PlaceHolders::_1)))
, d_retryStrategy(retryStrategy)
, d_retryCallback()
, d_onError(errorCb)
, d_onSuccess(successCb)
{
}

void RetryHandler::retry(const RetryCallback& onRetry)
{
    d_retryCallback = onRetry;
    scheduleRetry();
}

void RetryHandler::success() { d_retryStrategy->success(); }

void RetryHandler::handleRetry(Timer::InterruptReason reason)
{
    if (reason == Timer::CANCEL) {
        // Cancelled
        BALL_LOG_DEBUG << "Aborted retrying operation";
        return;
    }

    BALL_LOG_DEBUG << "Retry timer triggered";
    d_retryStrategy->attempt();
    d_retryCallback();
}

void RetryHandler::scheduleRetry()
{
    BALL_LOG_DEBUG << "RetryStrategy: " << *d_retryStrategy;
    d_sleepTimer->reset(d_retryStrategy->getNextRetryInterval());
}

} // namespace rmqio
} // namespace BloombergLP
