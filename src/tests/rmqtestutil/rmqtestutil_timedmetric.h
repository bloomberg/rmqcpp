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

#ifndef INCLUDED_RMQTESTUTIL_TIMEDMETRIC
#define INCLUDED_RMQTESTUTIL_TIMEDMETRIC

#include <bdlt_currenttime.h>
#include <bsl_iostream.h>
#include <bsls_timeinterval.h>

namespace BloombergLP {
namespace rmqtestutil {

template <class Metric>
class TimedMetric {
  public:
    explicit TimedMetric(const Metric& n)
    : d_snapshotTime(bdlt::CurrentTime::now())
    , d_snapshot(n)
    , d_metric(n)
    {
    }

    double currentRate()
    {
        double timeDiff = currentTimeDiffSeconds();
        return timeDiff > 0 ? currentDiff() / timeDiff : 0;
    }

    double currentDiff() { return d_metric - d_snapshot; }

    double currentTimeDiffSeconds()
    {
        return (bdlt::CurrentTime::now() - d_snapshotTime)
            .totalSecondsAsDouble();
    }

    ~TimedMetric()
    {
        bsl::cout << "Total " << currentDiff() << " messages are processed in "
                  << currentTimeDiffSeconds() << " seconds"
                  << ", rate: " << currentRate() << " messages/s \n";
    }

  private:
    const bsls::TimeInterval d_snapshotTime;
    const double d_snapshot;
    const Metric& d_metric;
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif
