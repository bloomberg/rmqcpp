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

#ifndef INCLUDED_RMQTESTUTIL_MOCKMETRICPUBLISHER
#define INCLUDED_RMQTESTUTIL_MOCKMETRICPUBLISHER

#include <rmqp_metricpublisher.h>

#include <bsl_utility.h>
#include <bsl_vector.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace BloombergLP {
namespace rmqtestutil {
class MockMetricPublisher : public rmqp::MetricPublisher {
  public:
    MOCK_METHOD3(
        publishGauge,
        void(const bsl::string&,
             double,
             const bsl::vector<bsl::pair<bsl::string, bsl::string> >&));
    MOCK_METHOD3(
        publishCounter,
        void(const bsl::string&,
             double,
             const bsl::vector<bsl::pair<bsl::string, bsl::string> >&));
    MOCK_METHOD3(
        publishSummary,
        void(const bsl::string&,
             double,
             const bsl::vector<bsl::pair<bsl::string, bsl::string> >&));
    MOCK_METHOD3(
        publishDistribution,
        void(const bsl::string&,
             double,
             const bsl::vector<bsl::pair<bsl::string, bsl::string> >&));
};
} // namespace rmqtestutil
} // namespace BloombergLP

#endif
