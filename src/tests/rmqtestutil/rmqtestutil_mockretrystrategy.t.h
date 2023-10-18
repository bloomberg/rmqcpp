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

#ifndef INCLUDED_RMQTESTUTIL_MOCKRETRYSTRATEGY_T
#define INCLUDED_RMQTESTUTIL_MOCKRETRYSTRATEGY_T

#include <rmqio_retrystrategy.h>

#include <bsl_memory.h>

#include <bsl_ostream.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace BloombergLP {
namespace rmqtestutil {

class MockRetryStrategy : public rmqio::RetryStrategy {
  public:
    MOCK_METHOD0(attempt, void());
    MOCK_METHOD0(success, void());
    MOCK_METHOD0(getNextRetryInterval, bsls::TimeInterval());
    MOCK_CONST_METHOD1(print, bsl::ostream&(bsl::ostream&));
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif
