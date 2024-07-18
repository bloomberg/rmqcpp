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

#ifndef INCLUDED_RMQTESTUTIL_MOCKRETRYHANDLER_T
#define INCLUDED_RMQTESTUTIL_MOCKRETRYHANDLER_T

#include <rmqio_retryhandler.h>
#include <rmqt_result.h>
#include <rmqtestutil_mockretrystrategy.t.h>
#include <rmqtestutil_mocktimerfactory.h>

#include <bsl_memory.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace BloombergLP {
namespace rmqtestutil {

class MockRetryHandler : public rmqio::RetryHandler {
  public:
    explicit MockRetryHandler()
    : rmqio::RetryHandler(bsl::make_shared<rmqtestutil::MockTimerFactory>(),
                          rmqt::ErrorCallback(),
                          rmqt::SuccessCallback(),
                          bsl::make_shared<rmqtestutil::MockRetryStrategy>())
    {
    }

    MOCK_METHOD1(retry, void(const rmqio::RetryHandler::RetryCallback&));
    MOCK_CONST_METHOD0(errorCallback, const rmqt::ErrorCallback&());
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif
