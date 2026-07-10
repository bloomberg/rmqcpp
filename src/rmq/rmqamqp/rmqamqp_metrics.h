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

#ifndef INCLUDED_RMQAMQP_METRICS
#define INCLUDED_RMQAMQP_METRICS

//@PURPOSE: Hold information required for publishing metrics
//
//@CLASSES:
//  rmqamqp::Metrics: Store constants/methods useful for publishing metrics
//

namespace BloombergLP {
namespace rmqamqp {

class Metrics {
  public:
    static const char* NAMESPACE;
    static const char* VHOST_TAG;
    static const char* CHANNELTYPE_TAG;

    // Pause/Resume metrics
    static const char* CONSUMER_PAUSE_TOTAL;
    static const char* CONSUMER_RESUME_TOTAL;
    static const char* PAUSE_OPERATION_LATENCY_SECONDS;
    static const char* RESUME_OPERATION_LATENCY_SECONDS;

    // Health polling metrics
    static const char* HEALTH_CHECK_STATUS;
    static const char* HEALTH_CHECK_TOTAL;
    static const char* HEALTH_CHECK_FAILURES_TOTAL;
    static const char* HEALTH_CHECK_CONSECUTIVE_FAILURES;
    static const char* HEALTH_CHECK_DURATION_MS;
    static const char* HEALTH_TRIGGERED_PAUSE_TOTAL;
    static const char* HEALTH_TRIGGERED_RESUME_TOTAL;

    // Host Health awareness metrics
    static const char* HEALTH_AWARE_VHOST_CREATED;
    static const char* HEALTH_UNAWARE_VHOST_CREATED;
    static const char* HEALTH_AWARE_CONSUMER_CREATED;
    static const char* HEALTH_UNAWARE_CONSUMER_CREATED;
    static const char* HEALTH_CHECK_BLOCKED_EVENT_LOOP;
    static const char* HEALTH_AWARE_VHOSTS;
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
