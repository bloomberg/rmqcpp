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

#include <rmqa_rabbitcontextoptions.h>

#include <rmqamqpt_constants.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bdls_osutil.h>
#include <bdls_processutil.h>
#include <bsls_assert.h>
#include <bsls_timeinterval.h>

namespace BloombergLP {
namespace rmqa {

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.RABBITCONTEXTOPTIONS")

namespace {

void populateUsefulInformation(rmqt::FieldTable* propertiesPtr)
{
    rmqt::FieldTable& properties = *propertiesPtr;

    int pid           = bdls::ProcessUtil::getProcessId();
    properties["pid"] = rmqt::FieldValue(pid);

    bsl::string processPath = "unknown";
    bdls::ProcessUtil::getPathToExecutable(&processPath);
    properties["task"] = rmqt::FieldValue(processPath);

    bsl::string osName = "unknown", osVersion = "unknown", osPatch = "unknown";
    bdls::OsUtil::getOsInfo(&osName, &osVersion, &osPatch);
    properties["os"]         = rmqt::FieldValue(osName);
    properties["os_version"] = rmqt::FieldValue(osVersion);
    properties["os_patch"]   = rmqt::FieldValue(osPatch);
}

void defaultErrorCallback(const bsl::string& err, int rc)
{
    BALL_LOG_SET_CATEGORY("rmqa::defaultErrorCallback");
    if (rc == static_cast<int>(rmqamqpt::Constants::CONNECTION_FORCED)) {
        BALL_LOG_INFO << "Error: " << err << ", rc: " << rc;
    }
    else {
        BALL_LOG_ERROR << "Error: " << err << ", rc: " << rc;
    }
}
} // namespace

RabbitContextOptions::RabbitContextOptions()
: d_onError(bdlf::BindUtil::bind(&defaultErrorCallback,
                                 bdlf::PlaceHolders::_1,
                                 bdlf::PlaceHolders::_2))
, d_hostHealthSelection(HostHealthAwarenessUnset())
, d_threadpool()
, d_metricPublisher()
, d_messageProcessingTimeout(DEFAULT_MESSAGE_PROCESSING_TIMEOUT)
, d_connectionErrorThreshold()
, d_connectionEstablishmentTimeout()
, d_clientProperties()
, d_tunables()
, d_shuffleConnectionEndpoints()
{
    populateUsefulInformation(&d_clientProperties);
}

RabbitContextOptions&
RabbitContextOptions::setThreadpool(bdlmt::ThreadPool* threadpool)
{
    d_threadpool = threadpool;
    return *this;
}

RabbitContextOptions&
RabbitContextOptions::setErrorCallback(const rmqt::ErrorCallback& errorCallback)
{
    d_onError = errorCallback;
    return *this;
}

RabbitContextOptions& RabbitContextOptions::setMetricPublisher(
    const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher)
{
    d_metricPublisher = metricPublisher;
    return *this;
}

RabbitContextOptions&
RabbitContextOptions::setClientProperty(const bsl::string& name,
                                        const rmqt::FieldValue& value)
{
    d_clientProperties[name] = value;
    return *this;
}

RabbitContextOptions& RabbitContextOptions::setMessageProcessingTimeout(
    const bsls::TimeInterval& timeout)
{
    d_messageProcessingTimeout = timeout;
    return *this;
}

RabbitContextOptions& RabbitContextOptions::setConnectionErrorThreshold(
    const bsl::optional<bsls::TimeInterval>& timeout)
{
    d_connectionErrorThreshold = timeout;
    return *this;
}

RabbitContextOptions& RabbitContextOptions::setConnectionEstablishmentTimeout(
    const bsl::optional<bsls::TimeInterval>& timeout)
{
    // The hung timer applies this at millisecond granularity (it uses
    // bsls::TimeInterval::totalMilliseconds()), so any value below 1ms --
    // including a positive sub-millisecond interval -- rounds down to zero and
    // would expire the bound immediately on every attempt, producing a
    // connect -> hung -> retry loop that never establishes. Require at least
    // 1ms rather than merely positive; log as well as assert so the
    // misconfiguration is diagnosable even in builds where assertions are
    // compiled out.
    if (timeout.has_value() && *timeout < bsls::TimeInterval(0, 1000 * 1000)) {
        BALL_LOG_ERROR << "setConnectionEstablishmentTimeout given "
                       << (timeout->totalSecondsAsDouble() * 1000.0)
                       << "ms; values below 1ms round down to zero at the "
                          "timer's millisecond granularity and prevent any "
                          "connection from establishing";
    }
    BSLS_ASSERT(!timeout.has_value() ||
                *timeout >= bsls::TimeInterval(0, 1000 * 1000));
    d_connectionEstablishmentTimeout = timeout;
    return *this;
}

RabbitContextOptions& RabbitContextOptions::setConsumerTracing(
    const bsl::shared_ptr<rmqp::ConsumerTracing>& consumerTracing)
{
    d_consumerTracing = consumerTracing;
    return *this;
}

RabbitContextOptions& RabbitContextOptions::setProducerTracing(
    const bsl::shared_ptr<rmqp::ProducerTracing>& producerTracing)
{
    d_producerTracing = producerTracing;
    return *this;
}

RabbitContextOptions& RabbitContextOptions::useRabbitMQFieldValueEncoding(bool)
{
    return *this;
}

RabbitContextOptions& RabbitContextOptions::setShuffleConnectionEndpoints(
    bool shuffleConnectionEndpoints)
{
    d_shuffleConnectionEndpoints = shuffleConnectionEndpoints;
    return *this;
}

RabbitContextOptions& RabbitContextOptions::setHostHealthConfig(
    const rmqt::HostHealthConfig& hostHealthConfig)
{
    // A config is one alternative of the selection; delegate so the variant
    // setter remains the single writer of d_hostHealthSelection.
    return setHostHealthSelection(hostHealthConfig);
}

RabbitContextOptions& RabbitContextOptions::setHostHealthSelection(
    const HostHealthSelection& selection)
{
    d_hostHealthSelection = selection;
    return *this;
}

#ifdef USES_LIBRMQ_EXPERIMENTAL_FEATURES
RabbitContextOptions&
RabbitContextOptions::setTunable(const bsl::string& tunable)
{
    d_tunables.insert(tunable);
    return *this;
}
#endif

} // namespace rmqa
} // namespace BloombergLP
