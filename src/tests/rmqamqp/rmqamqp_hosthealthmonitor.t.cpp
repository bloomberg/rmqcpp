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

#include <rmqamqp_hosthealthmonitor.h>

#include <rmqamqp_connection.h>
#include <rmqamqp_heartbeatmanagerimpl.h>
#include <rmqamqp_metrics.h>
#include <rmqio_backofflevelretrystrategy.h>
#include <rmqio_retryhandler.h>
#include <rmqt_hosthealthconfig.h>
#include <rmqt_plaincredentials.h>
#include <rmqt_simpleendpoint.h>
#include <rmqtestutil_mockmetricpublisher.h>
#include <rmqtestutil_mockresolver.t.h>
#include <rmqtestutil_mocktimerfactory.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bdlf_bind.h>
#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_sstream.h>
#include <bsl_stdexcept.h>
#include <bslmt_threadutil.h>

using namespace BloombergLP;
using namespace rmqamqp;
using namespace ::testing;

namespace {
class MockConnection : public rmqamqp::Connection {
  public:
    MockConnection(
        const bsl::shared_ptr<rmqio::Resolver>& resolver,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const bsl::shared_ptr<rmqamqp::HeartbeatManager>& hbManager,
        const bsl::shared_ptr<rmqtestutil::MockTimerFactory>& hungTimerFactory,
        const bsl::shared_ptr<rmqamqp::ChannelFactory>& channelFactory,
        const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
        const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
        const bsl::shared_ptr<rmqt::Credentials>& credentials,
        const rmqt::FieldTable& clientProperties,
        const bsl::string& connectionName)
    : rmqamqp::Connection(resolver,
                          retryHandler,
                          hbManager,
                          hungTimerFactory,
                          channelFactory,
                          metricPublisher,
                          endpoint,
                          credentials,
                          clientProperties,
                          bsl::optional<bsls::TimeInterval>(),
                          connectionName)
    {
    }

    MOCK_METHOD1(pauseReceiveChannels, void(bool));
    MOCK_METHOD1(resumeReceiveChannels, void(bool));
};
} // namespace

class HostHealthMonitorTests : public Test {
  public:
    struct ConfigurableHealthChecker {
        bool d_nextResult;
        bool d_throwBslException;
        bool d_throwUnknown;
        int d_sleepMicroseconds;

        ConfigurableHealthChecker()
        : d_nextResult(true)
        , d_throwBslException(false)
        , d_throwUnknown(false)
        , d_sleepMicroseconds(0)
        {
        }

        bool operator()()
        {
            if (d_sleepMicroseconds > 0) {
                bslmt::ThreadUtil::microSleep(d_sleepMicroseconds);
            }
            if (d_throwBslException) {
                throw bsl::runtime_error("checker failure");
            }
            if (d_throwUnknown) {
                throw 42;
            }
            return d_nextResult;
        }
    };

    bsl::shared_ptr<rmqtestutil::MockResolver> d_resolver;
    bsl::shared_ptr<rmqtestutil::MockTimerFactory> d_timerFactory;
    rmqt::ErrorCallback d_onError;
    bsl::shared_ptr<rmqio::RetryHandler> d_retryHandler;
    bsl::shared_ptr<rmqamqp::HeartbeatManager> d_hb;
    bsl::shared_ptr<rmqamqp::ChannelFactory> d_channelFactory;
    bsl::shared_ptr<rmqtestutil::MockMetricPublisher> d_metricPublisher;
    bsl::shared_ptr<rmqt::Endpoint> d_endpoint;
    bsl::shared_ptr<rmqt::Credentials> d_credentials;

    ConfigurableHealthChecker d_configurableHealthChecker;

    rmqt::HostHealthConfig d_config;
    bsl::shared_ptr<HostHealthMonitor> d_monitor;
    bsl::shared_ptr<MockConnection> d_connection;

    HostHealthMonitorTests()
    : d_resolver(bsl::make_shared<rmqtestutil::MockResolver>())
    , d_timerFactory(bsl::make_shared<rmqtestutil::MockTimerFactory>())
    , d_onError()
    , d_retryHandler(bsl::make_shared<rmqio::RetryHandler>(
          d_timerFactory,
          d_onError,
          bsl::make_shared<rmqio::BackoffLevelRetryStrategy>()))
    , d_hb(new rmqamqp::HeartbeatManagerImpl(d_timerFactory))
    , d_channelFactory(bsl::make_shared<rmqamqp::ChannelFactory>())
    , d_metricPublisher(bsl::make_shared<rmqtestutil::MockMetricPublisher>())
    , d_endpoint(new rmqt::SimpleEndpoint("", ""))
    , d_credentials(new rmqt::PlainCredentials("", ""))
    , d_config(makeConfig())
    , d_monitor()
    , d_connection(makeConnection("host-health-connection"))
    {
        d_monitor = bsl::make_shared<HostHealthMonitor>(
            d_config, d_metricPublisher.get());
        d_monitor->start(d_timerFactory);

        // Expect gauge metrics (health_aware_vhosts, health_check_status, etc.)
        EXPECT_CALL(*d_metricPublisher, publishGauge(_, _, _))
            .Times(AtLeast(0));

        d_monitor->registerConnection(
            bsl::weak_ptr<rmqamqp::Connection>(d_connection));
    }

    ~HostHealthMonitorTests() {}

    void stepOnePollInterval()
    {
        d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
    }

    /// Expect metrics for a health check that throws exception (RETRY state)
    /// consecutiveFailures is the expected value for the consecutive failures
    /// gauge
    void expectRetryCheckMetrics(double consecutiveFailures)
    {
        EXPECT_CALL(
            *d_metricPublisher,
            publishCounter(
                bsl::string(rmqamqp::Metrics::HEALTH_CHECK_TOTAL), 1.0, _))
            .Times(1);
        EXPECT_CALL(
            *d_metricPublisher,
            publishCounter(
                bsl::string(rmqamqp::Metrics::HEALTH_CHECK_FAILURES_TOTAL),
                1.0,
                _))
            .Times(1);
        EXPECT_CALL(
            *d_metricPublisher,
            publishGauge(
                bsl::string(
                    rmqamqp::Metrics::HEALTH_CHECK_CONSECUTIVE_FAILURES),
                consecutiveFailures,
                _))
            .Times(1);
        EXPECT_CALL(*d_metricPublisher,
                    publishCounter(bsl::string("disconnect_events"), _, _))
            .Times(AnyNumber());
    }

    void stepAndClear()
    {
        stepOnePollInterval();
        Mock::VerifyAndClearExpectations(d_connection.get());
    }

    bsl::shared_ptr<MockConnection> makeConnection(const bsl::string& name)
    {
        rmqt::FieldTable clientProps;
        clientProps["connection_name"] = rmqt::FieldValue(name);

        bsl::shared_ptr<NiceMock<MockConnection> > mc =
            bsl::make_shared<NiceMock<MockConnection> >(d_resolver,
                                                        d_retryHandler,
                                                        d_hb,
                                                        d_timerFactory,
                                                        d_channelFactory,
                                                        d_metricPublisher,
                                                        d_endpoint,
                                                        d_credentials,
                                                        clientProps,
                                                        name);

        return mc;
    }

    rmqt::HostHealthConfig makeConfig()
    {
        const uint16_t pollIntervalSeconds = 1;
        const uint16_t maxRetriesOnFailure = 3;

        return rmqt::HostHealthConfig(
            bdlf::BindUtil::bind(&ConfigurableHealthChecker::operator(),
                                 &d_configurableHealthChecker),
            pollIntervalSeconds,
            maxRetriesOnFailure);
    }
};

TEST_F(HostHealthMonitorTests, HealthyHostResumesConnections)
{
    d_configurableHealthChecker.d_nextResult = true;

    EXPECT_CALL(*d_connection, resumeReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, FirstCheckFiresImmediately)
{
    d_configurableHealthChecker.d_nextResult = true;

    EXPECT_CALL(*d_connection, resumeReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);

    // First check is scheduled with zero delay, so stepping by zero fires it.
    d_timerFactory->step_time(bsls::TimeInterval(0));
}

TEST_F(HostHealthMonitorTests, UnhealthyHostPausesConnections)
{
    d_configurableHealthChecker.d_nextResult = false;

    EXPECT_CALL(*d_connection, pauseReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, RegisterOnUnhealthyHostPausesImmediately)
{
    // Drive one check that marks the host UNHEALTHY.
    d_configurableHealthChecker.d_nextResult = false;

    EXPECT_CALL(*d_connection, pauseReceiveChannels(true)).Times(1);
    stepAndClear();

    bsl::shared_ptr<MockConnection> lateConn = makeConnection("late-unhealthy");

    EXPECT_CALL(*lateConn, pauseReceiveChannels(true)).Times(1);
    EXPECT_CALL(*lateConn, resumeReceiveChannels(_)).Times(0);

    d_monitor->registerConnection(bsl::weak_ptr<rmqamqp::Connection>(lateConn));
}

TEST_F(HostHealthMonitorTests, RegisterOnHealthyHostResumesImmediately)
{
    // Drive one check that marks the host HEALTHY.
    d_configurableHealthChecker.d_nextResult = true;

    EXPECT_CALL(*d_connection, resumeReceiveChannels(true)).Times(1);
    stepAndClear();

    bsl::shared_ptr<MockConnection> lateConn = makeConnection("late-healthy");

    EXPECT_CALL(*lateConn, resumeReceiveChannels(true)).Times(1);
    EXPECT_CALL(*lateConn, pauseReceiveChannels(_)).Times(0);

    d_monitor->registerConnection(bsl::weak_ptr<rmqamqp::Connection>(lateConn));
}

TEST_F(HostHealthMonitorTests, RegisterBeforeFirstCheckPausesImmediately)
{
    // No check has run yet, so the monitor's fail-safe default (UNHEALTHY)
    // applies.
    bsl::shared_ptr<HostHealthMonitor> monitor =
        bsl::make_shared<HostHealthMonitor>(d_config, d_metricPublisher.get());
    monitor->start(d_timerFactory);

    EXPECT_CALL(*d_metricPublisher, publishGauge(_, _, _)).Times(AtLeast(0));

    bsl::shared_ptr<MockConnection> earlyConn = makeConnection("early");

    EXPECT_CALL(*earlyConn, pauseReceiveChannels(true)).Times(1);
    EXPECT_CALL(*earlyConn, resumeReceiveChannels(_)).Times(0);

    monitor->registerConnection(bsl::weak_ptr<rmqamqp::Connection>(earlyConn));
}

TEST_F(HostHealthMonitorTests, ExpiredConnectionIsRemovedAndNotUsed)
{
    bsl::shared_ptr<MockConnection> liveConn =
        makeConnection("connection-live");
    bsl::shared_ptr<MockConnection> deadConn =
        makeConnection("connection-dead");

    bsl::shared_ptr<HostHealthMonitor> monitor =
        bsl::make_shared<HostHealthMonitor>(d_config, d_metricPublisher.get());
    monitor->start(d_timerFactory);

    // Expect gauge metrics on registration and cleanup
    EXPECT_CALL(*d_metricPublisher, publishGauge(_, _, _)).Times(AtLeast(0));

    monitor->registerConnection(bsl::weak_ptr<rmqamqp::Connection>(liveConn));
    monitor->registerConnection(bsl::weak_ptr<rmqamqp::Connection>(deadConn));

    bsl::weak_ptr<MockConnection> weakDead = deadConn;
    deadConn.reset();
    ASSERT_TRUE(weakDead.expired());

    d_configurableHealthChecker.d_nextResult = true;

    EXPECT_CALL(*liveConn, resumeReceiveChannels(true)).Times(1);
    EXPECT_CALL(*liveConn, pauseReceiveChannels(_)).Times(0);

    d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
}

TEST_F(HostHealthMonitorTests, BslExceptionRetriesUntilMaxThenPauses)
{
    d_configurableHealthChecker.d_throwBslException = true;

    // For first attempt and d_maxRetriesOnFailure retries, hostHealth == RETRY.
    // No pause/resume expected.
    for (unsigned short i = 0; i <= d_config.maxRetriesOnFailure(); ++i) {
        EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);
        EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

        stepAndClear();
    }

    // Next run: getHostHealth should return UNHEALTHY, and run() should pause.
    EXPECT_CALL(*d_connection, pauseReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, UnknownExceptionRetriesUntilMaxThenPauses)
{
    d_configurableHealthChecker.d_throwUnknown = true;

    for (unsigned short i = 0; i <= d_config.maxRetriesOnFailure(); ++i) {
        EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);
        EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

        stepAndClear();
    }

    EXPECT_CALL(*d_connection, pauseReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests,
       BslExceptionThenHealthyMarksHostHealthyAndResumes)
{
    d_configurableHealthChecker.d_throwBslException = true;

    // First two runs: RETRY, no pause/resume.
    for (unsigned short i = 0; i < 2; ++i) {
        EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);
        EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

        stepAndClear();
    }

    // Now checker stops throwing and returns healthy.
    d_configurableHealthChecker.d_throwBslException = false;
    d_configurableHealthChecker.d_nextResult        = true;

    EXPECT_CALL(*d_connection, resumeReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests,
       BslExceptionThenUnhealthyMarksHostUnhealthyAndPauses)
{
    d_configurableHealthChecker.d_throwBslException = true;

    // First run: RETRY, no pause/resume.
    EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);
    EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

    stepAndClear();

    // Now checker stops throwing and returns false (unhealthy).
    d_configurableHealthChecker.d_throwBslException = false;
    d_configurableHealthChecker.d_nextResult        = false;

    EXPECT_CALL(*d_connection, pauseReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, DestructionWhileTimerPending)
{
    bsl::shared_ptr<HostHealthMonitor> monitor =
        bsl::make_shared<HostHealthMonitor>(d_config, d_metricPublisher.get());
    monitor->start(d_timerFactory);

    EXPECT_CALL(*d_metricPublisher, publishGauge(_, _, _)).Times(AtLeast(0));

    bsl::shared_ptr<MockConnection> conn = makeConnection("destroy-test");
    monitor->registerConnection(bsl::weak_ptr<rmqamqp::Connection>(conn));

    monitor.reset();

    d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
    d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
}

TEST_F(HostHealthMonitorTests, StopPreventsFurtherCallbacks)
{
    d_configurableHealthChecker.d_nextResult = true;

    EXPECT_CALL(*d_connection, resumeReceiveChannels(true)).Times(1);
    stepAndClear();

    d_monitor->stop();

    EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);
    EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);

    d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
    d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
}

TEST_F(HostHealthMonitorTests, StartAfterStopOnSameInstance)
{
    d_configurableHealthChecker.d_nextResult = true;

    EXPECT_CALL(*d_connection, resumeReceiveChannels(true)).Times(1);
    stepAndClear();

    d_monitor->stop();

    EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);
    EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);
    d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
    Mock::VerifyAndClearExpectations(d_connection.get());

    d_monitor->start(d_timerFactory);

    EXPECT_CALL(*d_connection, resumeReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);

    d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
}

TEST_F(HostHealthMonitorTests, HostHealthStreamingOperator)
{
    {
        bsl::ostringstream oss;
        oss << HostHealthMonitor::HEALTHY;
        EXPECT_EQ(oss.str(), "HEALTHY");
    }
    {
        bsl::ostringstream oss;
        oss << HostHealthMonitor::UNHEALTHY;
        EXPECT_EQ(oss.str(), "UNHEALTHY");
    }
    {
        bsl::ostringstream oss;
        oss << HostHealthMonitor::RETRY;
        EXPECT_EQ(oss.str(), "RETRY");
    }
    {
        // Test unknown/default case by casting an invalid value
        bsl::ostringstream oss;
        oss << static_cast<HostHealthMonitor::HostHealth>(999);
        EXPECT_EQ(oss.str(), "UNKNOWN");
    }
}

TEST_F(HostHealthMonitorTests, PublishesHealthyCheckMetrics)
{
    d_configurableHealthChecker.d_nextResult = true;

    EXPECT_CALL(*d_metricPublisher,
                publishCounter(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_TOTAL), 1.0, _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishSummary(
            bsl::string(rmqamqp::Metrics::HEALTH_CHECK_DURATION_MS), _, _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishGauge(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_STATUS), 1.0, _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishGauge(
            bsl::string(rmqamqp::Metrics::HEALTH_CHECK_CONSECUTIVE_FAILURES),
            0.0,
            _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishCounter(
            bsl::string(rmqamqp::Metrics::HEALTH_TRIGGERED_RESUME_TOTAL),
            1.0,
            _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("disconnect_events"), _, _))
        .Times(AnyNumber());

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, PublishesUnhealthyCheckMetrics)
{
    d_configurableHealthChecker.d_nextResult = false;

    EXPECT_CALL(*d_metricPublisher,
                publishCounter(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_TOTAL), 1.0, _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishSummary(
            bsl::string(rmqamqp::Metrics::HEALTH_CHECK_DURATION_MS), _, _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishGauge(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_STATUS), 0.0, _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(
                    bsl::string(rmqamqp::Metrics::HEALTH_TRIGGERED_PAUSE_TOTAL),
                    1.0,
                    _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("disconnect_events"), _, _))
        .Times(AnyNumber());

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, PublishesRetryCheckMetricsOnBslException)
{
    d_configurableHealthChecker.d_throwBslException = true;

    expectRetryCheckMetrics(1.0);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, PublishesRetryCheckMetricsOnUnknownException)
{
    d_configurableHealthChecker.d_throwUnknown = true;

    expectRetryCheckMetrics(1.0);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests,
       PublishesConsecutiveFailuresGaugeDuringRetryPeriod)
{
    d_configurableHealthChecker.d_throwBslException = true;

    // First failure: consecutive failures = 1
    expectRetryCheckMetrics(1.0);
    stepAndClear();

    // Second failure: consecutive failures = 2
    expectRetryCheckMetrics(2.0);
    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests,
       PublishesConsecutiveFailuresGaugeWhenMaxRetriesReached)
{
    d_configurableHealthChecker.d_throwBslException = true;

    // Run checks that don't exceed max retries (RETRY state)
    for (unsigned short i = 0; i <= d_config.maxRetriesOnFailure(); ++i) {
        stepAndClear();
    }

    // This run will exceed maxRetries and transition to UNHEALTHY
    // Note: Due to post-increment, consecutive failures will be
    // maxRetriesOnFailure + 2
    const double expectedFailures =
        static_cast<double>(d_config.maxRetriesOnFailure() + 2);
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_TOTAL), 1.0, _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishCounter(
            bsl::string(rmqamqp::Metrics::HEALTH_CHECK_FAILURES_TOTAL), 1.0, _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishGauge(
            bsl::string(rmqamqp::Metrics::HEALTH_CHECK_CONSECUTIVE_FAILURES),
            expectedFailures,
            _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishGauge(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_STATUS), 0.0, _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(
                    bsl::string(rmqamqp::Metrics::HEALTH_TRIGGERED_PAUSE_TOTAL),
                    1.0,
                    _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("disconnect_events"), _, _))
        .Times(AnyNumber());

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, BlockedEventLoopMetricPublished)
{
    // This test takes ~1.1s: exercises real wall-clock duration detection
    d_configurableHealthChecker.d_nextResult        = true;
    d_configurableHealthChecker.d_sleepMicroseconds = 1100000; // 1.1 seconds

    EXPECT_CALL(
        *d_metricPublisher,
        publishCounter(
            bsl::string(rmqamqp::Metrics::HEALTH_CHECK_BLOCKED_EVENT_LOOP),
            1.0,
            _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_TOTAL), 1.0, _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishSummary(
            bsl::string(rmqamqp::Metrics::HEALTH_CHECK_DURATION_MS), _, _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishCounter(
            bsl::string(rmqamqp::Metrics::HEALTH_TRIGGERED_RESUME_TOTAL),
            1.0,
            _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("disconnect_events"), _, _))
        .Times(AnyNumber());

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, RetrySchedulesNextCheck)
{
    d_configurableHealthChecker.d_throwBslException = true;

    // First step: RETRY path — scheduleNextCheck() runs at top of checkHealth()
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_TOTAL), 1.0, _))
        .Times(1);
    EXPECT_CALL(
        *d_metricPublisher,
        publishCounter(
            bsl::string(rmqamqp::Metrics::HEALTH_CHECK_FAILURES_TOTAL), 1.0, _))
        .Times(1);
    EXPECT_CALL(*d_metricPublisher, publishGauge(_, _, _)).Times(AtLeast(0));
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(bsl::string("disconnect_events"), _, _))
        .Times(AnyNumber());
    EXPECT_CALL(*d_connection, pauseReceiveChannels(_)).Times(0);
    EXPECT_CALL(*d_connection, resumeReceiveChannels(_)).Times(0);

    stepOnePollInterval();
    Mock::VerifyAndClearExpectations(d_metricPublisher.get());
    Mock::VerifyAndClearExpectations(d_connection.get());

    // Second step: timer fires again — proves scheduleNextCheck() was called
    // Catch-alls first (lowest priority in gmock reverse-order matching)
    EXPECT_CALL(*d_metricPublisher, publishCounter(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*d_metricPublisher, publishGauge(_, _, _)).Times(AtLeast(0));
    // Specific expectation last (highest priority — checked first by gmock)
    EXPECT_CALL(*d_metricPublisher,
                publishCounter(
                    bsl::string(rmqamqp::Metrics::HEALTH_CHECK_TOTAL), 1.0, _))
        .Times(1);

    stepOnePollInterval();
}

TEST_F(HostHealthMonitorTests, RegisterConnectionAfterStopIsValid)
{
    d_monitor->stop();

    bsl::shared_ptr<MockConnection> conn = makeConnection("late-register");

    EXPECT_CALL(
        *d_metricPublisher,
        publishGauge(bsl::string(rmqamqp::Metrics::HEALTH_AWARE_VHOSTS), _, _))
        .Times(1);

    d_monitor->registerConnection(bsl::weak_ptr<rmqamqp::Connection>(conn));

    // Restart: late-registered connection should be notified
    d_configurableHealthChecker.d_nextResult = true;
    d_monitor->start(d_timerFactory);

    EXPECT_CALL(*d_connection, resumeReceiveChannels(true)).Times(1);
    EXPECT_CALL(*conn, resumeReceiveChannels(true)).Times(1);
    EXPECT_CALL(*d_metricPublisher, publishGauge(_, _, _)).Times(AtLeast(0));
    EXPECT_CALL(*d_metricPublisher, publishCounter(_, _, _)).Times(AtLeast(0));
    EXPECT_CALL(*d_metricPublisher, publishSummary(_, _, _)).Times(AtLeast(0));

    d_timerFactory->step_time(bsls::TimeInterval(d_config.pollInterval()));
}
