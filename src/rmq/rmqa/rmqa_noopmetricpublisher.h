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

#ifndef INCLUDED_RMQA_NOOPMETRICPUBLISHER
#define INCLUDED_RMQA_NOOPMETRICPUBLISHER

#include <rmqp_metricpublisher.h>

#include <ball_log.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqa {

/// \brief Default metric publisher
///
/// NoOpMetricPublisher is the default metric publisher. When passed to
/// `rmqa::RabbitContext` as argument, `rmqcpp` internal metrics won't be
/// published.

class NoOpMetricPublisher : public rmqp::MetricPublisher {
  public:
    NoOpMetricPublisher()
    {
        BALL_LOG_SET_CATEGORY("RMQA.NOOPMETRICPUBLISHER");
        BALL_LOG_INFO << "Initialised NoOpMetricPublisher (metrics will not be "
                         "published)";
    }

    ~NoOpMetricPublisher() BSLS_KEYWORD_OVERRIDE {}

    virtual void
    publishGauge(const bsl::string&,
                 double,
                 const bsl::vector<bsl::pair<bsl::string, bsl::string> >&)
        BSLS_KEYWORD_OVERRIDE
    {
    }

    virtual void
    publishCounter(const bsl::string&,
                   double,
                   const bsl::vector<bsl::pair<bsl::string, bsl::string> >&)
        BSLS_KEYWORD_OVERRIDE
    {
    }

    virtual void
    publishSummary(const bsl::string&,
                   double,
                   const bsl::vector<bsl::pair<bsl::string, bsl::string> >&)
        BSLS_KEYWORD_OVERRIDE
    {
    }

    virtual void publishDistribution(
        const bsl::string&,
        double,
        const bsl::vector<bsl::pair<bsl::string, bsl::string> >&)
        BSLS_KEYWORD_OVERRIDE
    {
    }
};
} // namespace rmqa
} // namespace BloombergLP

#endif
