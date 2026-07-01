// Copyright 2025 Bloomberg Finance L.P.
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

#include <rmqamqp_hosthealthmonitor.h>

#include <rmqamqp_metrics.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bsl_algorithm.h>
#include <bsl_exception.h>
#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_utility.h>
#include <bsl_vector.h>
#include <bsls_assert.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.HOSTHEALTHMONITOR")

const bool RESPECT_HOST_HEALTH = true;

} // namespace

HostHealthMonitor::HostHealthMonitor(
    const rmqt::HostHealthConfig& hostHealthConfig,
    rmqp::MetricPublisher* metricPublisher)
: d_hostHealthConfig(hostHealthConfig)
, d_currentTries(0)
, d_timer()
, d_metricPublisher(metricPublisher)
{
    BSLS_ASSERT(d_metricPublisher);
    BSLS_ASSERT(d_hostHealthConfig.pollInterval() > 0);
    BALL_LOG_INFO << "Monitoring host health using config " << hostHealthConfig
                  << " with health checks on the event loop";
}

HostHealthMonitor::~HostHealthMonitor()
{
    stop();
    d_connections.clear();
}

void HostHealthMonitor::start(
    const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory)
{
    BSLS_ASSERT_OPT(!d_timer);
    d_timer = timerFactory->createWithCallback(
        bdlf::BindUtil::bind(&HostHealthMonitor::handleTimerFired,
                             weak_from_this(),
                             bdlf::PlaceHolders::_1));
    scheduleNextCheck();
}

void HostHealthMonitor::stop()
{
    if (d_timer) {
        d_timer->cancel();
        d_timer.reset();
    }
}

void HostHealthMonitor::registerConnection(
    const bsl::weak_ptr<rmqamqp::Connection>& conn)
{
    d_connections.push_back(conn);

    d_metricPublisher->publishGauge(Metrics::HEALTH_AWARE_VHOSTS,
                                    static_cast<double>(d_connections.size()));
}

void HostHealthMonitor::handleTimerFired(
    const bsl::weak_ptr<HostHealthMonitor>& weakSelf,
    rmqio::Timer::InterruptReason reason)
{
    if (reason == rmqio::Timer::CANCEL) {
        return;
    }

    bsl::shared_ptr<HostHealthMonitor> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_DEBUG << "HostHealthMonitor destroyed before timer fired";
        return;
    }

    self->checkHealth();
}

void HostHealthMonitor::checkHealth()
{
    scheduleNextCheck();

    HostHealth result;

    d_metricPublisher->publishCounter(Metrics::HEALTH_CHECK_TOTAL, 1.0);

    try {
        const bsls::TimeInterval startTime =
            bsls::SystemTime::nowMonotonicClock();

        const bool healthCheckerResult = d_hostHealthConfig.healthChecker()();
        result = healthCheckerResult ? HEALTHY : UNHEALTHY;

        const bsls::TimeInterval endTime =
            bsls::SystemTime::nowMonotonicClock();
        const double durationMs = (endTime - startTime).totalMilliseconds();

        BALL_LOG_INFO << "Health check completed in " << durationMs
                      << " ms with result: " << healthCheckerResult
                      << ". Set health state to: " << result;

        d_metricPublisher->publishSummary(Metrics::HEALTH_CHECK_DURATION_MS,
                                          durationMs);

        const double HEALTHCHECK_DURATION_REPORTING_THRESHOLD =
            bsl::min(d_hostHealthConfig.pollInterval() * 1000.0 * 0.8, 1000.0);
        if (durationMs > HEALTHCHECK_DURATION_REPORTING_THRESHOLD) {
            BALL_LOG_WARN << "Host health check took " << durationMs
                          << " ms which exceeds the threshold of "
                          << HEALTHCHECK_DURATION_REPORTING_THRESHOLD << " ms.";
            d_metricPublisher->publishCounter(
                Metrics::HEALTH_CHECK_BLOCKED_EVENT_LOOP, 1.0);
        }
    }
    catch (const bsl::exception& e) {
        BALL_LOG_ERROR << "Host health check failed with exception: "
                       << e.what();
        result = RETRY;

        d_metricPublisher->publishCounter(Metrics::HEALTH_CHECK_FAILURES_TOTAL,
                                          1.0);
    }
    catch (...) {
        BALL_LOG_ERROR << "Host health check failed with unknown exception.";
        result = RETRY;

        d_metricPublisher->publishCounter(Metrics::HEALTH_CHECK_FAILURES_TOTAL,
                                          1.0);
    }

    if (result == RETRY) {
        if (d_currentTries++ > d_hostHealthConfig.maxRetriesOnFailure()) {
            BALL_LOG_ERROR << "Exceeded max retries on failure of "
                           << d_hostHealthConfig.maxRetriesOnFailure()
                           << ". Marking host as UNHEALTHY.";
            result = UNHEALTHY;

            d_metricPublisher->publishGauge(
                Metrics::HEALTH_CHECK_CONSECUTIVE_FAILURES,
                static_cast<double>(d_currentTries));
        }
        else {
            BALL_LOG_WARN << "Current tries " << d_currentTries
                          << " do not exceed max retries on failure of "
                          << d_hostHealthConfig.maxRetriesOnFailure()
                          << ". Will retry after "
                          << d_hostHealthConfig.pollInterval()
                          << " seconds. Will NOT pause the consumers yet.";

            if (d_metricPublisher) {
                d_metricPublisher->publishGauge(
                    Metrics::HEALTH_CHECK_CONSECUTIVE_FAILURES,
                    static_cast<double>(d_currentTries),
                    bsl::vector<bsl::pair<bsl::string, bsl::string> >());
            }
            return;
        }
    }

    processHealthResult(result);
}

void HostHealthMonitor::processHealthResult(HostHealth result)
{
    d_currentTries = 0;

    BALL_LOG_DEBUG << (result == HEALTHY ? "Resuming" : "Pausing")
                   << " host health aware consumers.";

    const double statusValue = (result == HEALTHY) ? 1.0 : 0.0;
    d_metricPublisher->publishGauge(Metrics::HEALTH_CHECK_STATUS, statusValue);

    if (result == HEALTHY) {
        d_metricPublisher->publishGauge(
            Metrics::HEALTH_CHECK_CONSECUTIVE_FAILURES, 0.0);
        d_metricPublisher->publishCounter(
            Metrics::HEALTH_TRIGGERED_RESUME_TOTAL, 1.0);
    }
    else {
        d_metricPublisher->publishCounter(Metrics::HEALTH_TRIGGERED_PAUSE_TOTAL,
                                          1.0);
    }

    for (bsl::list<bsl::weak_ptr<rmqamqp::Connection> >::iterator conn =
             d_connections.begin();
         conn != d_connections.end();) {

        bsl::shared_ptr<rmqamqp::Connection> connection = conn->lock();

        if (!connection) {
            BALL_LOG_DEBUG << "Remove destructed connection from host health "
                              "monitoring list.";
            conn = d_connections.erase(conn);

            d_metricPublisher->publishGauge(
                Metrics::HEALTH_AWARE_VHOSTS,
                static_cast<double>(d_connections.size()));

            continue;
        }

        if (result == HEALTHY) {
            connection->resumeReceiveChannels(RESPECT_HOST_HEALTH);
        }
        else {
            connection->pauseReceiveChannels(RESPECT_HOST_HEALTH);
        }

        ++conn;
    }
}

void HostHealthMonitor::scheduleNextCheck()
{
    d_timer->reset(bsls::TimeInterval(d_hostHealthConfig.pollInterval()));
}

bsl::ostream& operator<<(bsl::ostream& os,
                         HostHealthMonitor::HostHealth hostHealth)
{
    switch (hostHealth) {
        case HostHealthMonitor::HEALTHY:
            os << "HEALTHY";
            break;
        case HostHealthMonitor::UNHEALTHY:
            os << "UNHEALTHY";
            break;
        case HostHealthMonitor::RETRY:
            os << "RETRY";
            break;
        default:
            os << "UNKNOWN";
            break; // defensive fallback
    }
    return os;
}

} // namespace rmqamqp
} // namespace BloombergLP
