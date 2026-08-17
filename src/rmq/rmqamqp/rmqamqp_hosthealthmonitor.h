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

#ifndef INCLUDED_RMQA_HOSTHEALTHMONITOR
#define INCLUDED_RMQA_HOSTHEALTHMONITOR

#include <rmqamqp_connection.h>
#include <rmqio_timer.h>
#include <rmqp_metricpublisher.h>
#include <rmqt_hosthealthconfig.h>

#include <bsl_list.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqamqp {

/// \brief Monitors host health and pauses/resumes consumers accordingly
///
/// HostHealthMonitor periodically checks host health using a user-provided
/// health checker function. When the host becomes unhealthy, it pauses
/// message delivery on consumers that have \c consumeOnlyFromHealthyHost
/// enabled. When the host recovers, it resumes message delivery.
///
/// The health checker function runs on the main event loop and should
/// complete promptly, as it will delay AMQP heartbeats and message delivery
/// for all connections by its execution time every \c pollInterval seconds.
/// Each check schedules the next one via a timer (self-rescheduling pattern).
///
/// One monitor exists per context: the checker runs once per \c pollInterval
/// regardless of connection or consumer count. The latest result is cached and
/// applied to every registered connection (and to connections registering
/// between checks, see \c registerConnection). Defaults to unhealthy until the
/// first check.
class HostHealthMonitor
: public bsl::enable_shared_from_this<HostHealthMonitor> {
  public:
    /// Health state of the host
    /// HEALTHY: Host is healthy
    /// UNHEALTHY: Host is unhealthy
    /// RETRY: Host health checker failed due to some exception. Will retry
    ///        `HostHealthConfig::maxRetriesOnFailure()` times before
    ///        marking host as UNHEALTHY
    enum HostHealth { HEALTHY = 0, UNHEALTHY = 1, RETRY = 2 };

    // CREATORS

    /// Construct a HostHealthMonitor with the given configuration.
    ///
    /// \param hostHealthConfig Configuration for health checking
    /// \param metricPublisher Metric publisher for health check metrics.
    ///                        The behavior is undefined unless the supplied
    ///                        metric publisher remains valid for the lifetime
    ///                        of this object.
    HostHealthMonitor(const rmqt::HostHealthConfig& hostHealthConfig,
                      rmqp::MetricPublisher* metricPublisher);

    ~HostHealthMonitor();

    /// Start the health monitoring timer. The first health check is scheduled
    /// immediately on the event loop. Subsequent checks run every
    /// \c pollInterval seconds.
    void start(const bsl::shared_ptr<rmqio::TimerFactory>& timerFactory);

    /// Stop the health monitoring timer.
    void stop();

    /// Register a connection to be paused/resumed as host health changes.
    /// At registration the connection is brought into the current known state
    /// immediately: resumed if the host is known healthy, paused otherwise.
    void
    registerConnection(const bsl::weak_ptr<rmqamqp::Connection>& connection);

    friend bsl::ostream& operator<<(bsl::ostream& os, HostHealth hostHealth);

  private:
    static void
    handleTimerFired(const bsl::weak_ptr<HostHealthMonitor>& weakSelf,
                     rmqio::Timer::InterruptReason reason);
    void checkHealth();
    void processHealthResult(HostHealth result);
    void scheduleNextCheck();

    // DATA
    rmqt::HostHealthConfig d_hostHealthConfig;
    bsl::list<bsl::weak_ptr<rmqamqp::Connection> > d_connections;
    unsigned int d_currentTries;
    HostHealth d_latestHealthCheckResult;
    bsl::shared_ptr<rmqio::Timer> d_timer;
    rmqp::MetricPublisher* d_metricPublisher;
};

bsl::ostream& operator<<(bsl::ostream& os,
                         HostHealthMonitor::HostHealth hostHealth);

} // namespace rmqamqp
} // namespace BloombergLP

#endif
