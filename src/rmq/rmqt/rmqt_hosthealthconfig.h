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

// rmqt_hosthealthconfig.h
#ifndef INCLUDED_RMQT_HOSTHEALTHCONFIG
#define INCLUDED_RMQT_HOSTHEALTHCONFIG

#include <bsl_functional.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqt {

/// \brief Class for passing arguments to HostHealthMonitor
///
/// This class provides configuration for host health monitoring. When set via
/// \c RabbitContextOptions::setHostHealthConfig, a HostHealthMonitor is created
/// that periodically checks host health. Consumers with
/// \c ConsumerConfig::consumeOnlyFromHealthyHost enabled (the default) will
/// pause message delivery when the host is unhealthy and resume when healthy.
class HostHealthConfig {
  public:
    typedef bsl::function<bool()> HealthCheckerFunction;

    // Creators
    /// \param healthChecker A function that returns `true` if the host is
    ///        healthy, `false` if unhealthy. If the function throws, the
    ///        check is retried up to `maxRetriesOnFailure` times before
    ///        marking the host as unhealthy. This function runs on the main
    ///        event loop thread and should complete promptly, as it will
    ///        delay AMQP heartbeats and message delivery for all connections
    ///        by its execution time every `pollInterval` seconds.
    /// \param pollInterval The interval (in seconds) between each
    ///        health check
    /// \param maxRetriesOnFailure The maximum number of retries on failure
    ///        (exception) before marking host as unhealthy
    explicit HostHealthConfig(
        const HealthCheckerFunction& healthChecker,
        const uint16_t pollInterval        = s_defaultPollInterval,
        const uint16_t maxRetriesOnFailure = s_defaultMaxRetriesOnFailure);

    ~HostHealthConfig();

    // Getters
    HealthCheckerFunction healthChecker() const { return d_healthChecker; }

    uint16_t pollInterval() const { return d_pollInterval; }

    uint16_t maxRetriesOnFailure() const { return d_maxRetriesOnFailure; }

    // Setters
    /// \param pollInterval The interval (in seconds) between each health check
    HostHealthConfig& setPollInterval(const uint16_t pollInterval)
    {
        d_pollInterval = pollInterval;
        return *this;
    }

    /// \param maxRetriesOnFailure The maximum number of retries on failure
    ///        before marking host as unhealthy
    HostHealthConfig& setMaxRetriesOnFailure(const uint16_t maxRetriesOnFailure)
    {
        d_maxRetriesOnFailure = maxRetriesOnFailure;
        return *this;
    }

    friend bsl::ostream& operator<<(bsl::ostream& os,
                                    const HostHealthConfig& hostHealthConfig);

  private:
    static const uint16_t s_defaultPollInterval;
    static const uint16_t s_defaultMaxRetriesOnFailure;

    HealthCheckerFunction d_healthChecker;
    uint16_t d_pollInterval;
    uint16_t d_maxRetriesOnFailure;
};

bsl::ostream& operator<<(bsl::ostream& os,
                         const HostHealthConfig& hostHealthConfig);

} // namespace rmqt
} // namespace BloombergLP

#endif
