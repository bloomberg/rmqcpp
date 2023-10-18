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

#ifndef INCLUDED_RMQP_METRICPUBLISHER
#define INCLUDED_RMQP_METRICPUBLISHER

#include <bsl_string.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqp {

/// \class MetricPublisher
/// \brief An interface for publishing rmqcpp metrics.
///
/// The rmqcpp implementation uses MetricPublisher interface to publish various
/// runtime metrics. The users of rmqcpp can provide their own
/// implementation of this interface to receive the published metrics.
///
/// Every published metric consists of name, value and associated tags (e.g.
/// vhost name).
///
/// Implementation must be thread-safe.
class MetricPublisher {
  public:
    virtual ~MetricPublisher();

    /// Publish a gauge - the most recently observed value of a variable.
    virtual void publishGauge(
        const bsl::string& name,
        double value,
        const bsl::vector<bsl::pair<bsl::string, bsl::string> >& tags) = 0;

    /// Publish an increment to a counter variable.
    virtual void publishCounter(
        const bsl::string& name,
        double value,
        const bsl::vector<bsl::pair<bsl::string, bsl::string> >& tags) = 0;

    /// Publish a value for basic summary statistics.
    virtual void publishSummary(
        const bsl::string& name,
        double value,
        const bsl::vector<bsl::pair<bsl::string, bsl::string> >& tags) = 0;

    /// Publish a value for distribution statistics. A distribution is similar
    /// to a summary but also includes quantile statistics.
    virtual void publishDistribution(
        const bsl::string& name,
        double value,
        const bsl::vector<bsl::pair<bsl::string, bsl::string> >& tags) = 0;
};

} // namespace rmqp
} // namespace BloombergLP

#endif
