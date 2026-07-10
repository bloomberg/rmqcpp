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

#include <rmqamqp_metrics.h>

namespace BloombergLP {
namespace rmqamqp {

const char* Metrics::VHOST_TAG       = "rmqVhostName";
const char* Metrics::CHANNELTYPE_TAG = "rmqChannelType";

// Pause/Resume metrics
const char* Metrics::CONSUMER_PAUSE_TOTAL  = "consumer_pause_total";
const char* Metrics::CONSUMER_RESUME_TOTAL = "consumer_resume_total";
const char* Metrics::PAUSE_OPERATION_LATENCY_SECONDS =
    "pause_operation_latency_seconds";
const char* Metrics::RESUME_OPERATION_LATENCY_SECONDS =
    "resume_operation_latency_seconds";

// Health polling metrics
const char* Metrics::HEALTH_CHECK_STATUS = "health_check_status";
const char* Metrics::HEALTH_CHECK_TOTAL  = "health_check_total";
const char* Metrics::HEALTH_CHECK_FAILURES_TOTAL =
    "health_check_failures_total";
const char* Metrics::HEALTH_CHECK_CONSECUTIVE_FAILURES =
    "health_check_consecutive_failures";
const char* Metrics::HEALTH_CHECK_DURATION_MS = "health_check_duration_ms";
const char* Metrics::HEALTH_TRIGGERED_PAUSE_TOTAL =
    "health_triggered_pause_total";
const char* Metrics::HEALTH_TRIGGERED_RESUME_TOTAL =
    "health_triggered_resume_total";

const char* Metrics::HEALTH_CHECK_BLOCKED_EVENT_LOOP =
    "health_check_blocked_event_loop";

// Host Health awareness metrics
const char* Metrics::HEALTH_AWARE_VHOST_CREATED = "health_aware_vhost_created";
const char* Metrics::HEALTH_UNAWARE_VHOST_CREATED =
    "health_unaware_vhost_created";
const char* Metrics::HEALTH_AWARE_CONSUMER_CREATED =
    "health_aware_consumer_created";
const char* Metrics::HEALTH_UNAWARE_CONSUMER_CREATED =
    "health_unaware_consumer_created";
const char* Metrics::HEALTH_AWARE_VHOSTS = "health_aware_vhosts";
} // namespace rmqamqp
} // namespace BloombergLP
