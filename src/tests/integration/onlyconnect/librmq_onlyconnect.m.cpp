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

#include <rmqa_noopmetricpublisher.h>
#include <rmqa_rabbitcontextimpl.h>
#include <rmqa_rabbitcontextoptions.h>
#include <rmqintegration_testparameters.h>
#include <rmqio_asioeventloop.h>
#include <rmqio_eventloop.h>
#include <rmqt_securityparameters.h>

#include <balcl_commandline.h>
#include <ball_log.h>
#include <ball_loggermanager.h>
#include <ball_loggermanagerconfiguration.h>
#include <ball_severityutil.h>
#include <ball_streamobserver.h>
#include <bdlde_base64decoder.h>
#include <bsl_iostream.h>
#include <bslma_managedptr.h>
#include <bslmt_threadutil.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>

using namespace BloombergLP;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.ONLYCONNECT")

void errorCallback(const bsl::string& message, int code)
{
    bsl::cout << "ErrorCallback: " << message << " : " << code;
}

} // namespace

void configLog(const char* logLevel)
{
    static ball::LoggerManagerConfiguration configuration;
    static bsl::shared_ptr<ball::StreamObserver> loggingObserver =
        bsl::make_shared<ball::StreamObserver>(&bsl::cout);

    ball::Severity::Level level;
    if (ball::SeverityUtil::fromAsciiCaseless(&level, logLevel)) {
        configuration.setDefaultThresholdLevelsIfValid(ball::Severity::e_INFO);
    }
    else {
        configuration.setDefaultThresholdLevelsIfValid(level);
    }

    static ball::LoggerManagerScopedGuard guard(configuration);
    ball::LoggerManager::singleton().registerObserver(loggingObserver,
                                                      "stdout");
}

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    int exitDelay                = 0;
    int connectionErrorThreshold = 60 * 5; // 5 mins

    balcl::OptionInfo extraArgs[] = {
        {
            "threshold",
            "connectionErrorThreshold",
            "The connection error threshold, in seconds",
            balcl::TypeInfo(&connectionErrorThreshold),
            balcl::OccurrenceInfo(connectionErrorThreshold),
        },
        {
            "e|exit",
            "exit",
            "Delay this number of seconds before exiting, after connecting",
            balcl::TypeInfo(&exitDelay),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    rmqa::RabbitContextOptions options;
    options.setMetricPublisher(bsl::make_shared<rmqa::NoOpMetricPublisher>())
        .setClientProperty("ONLY_CONNECT",
                           rmqt::FieldValue(bsl::string("INTEGRATION_TEST")))
        .setConnectionErrorThreshold(
            bsls::TimeInterval(connectionErrorThreshold))
        .setErrorCallback(&errorCallback);

    rmqa::RabbitContextImpl context(
        bslma::ManagedPtr<rmqio::EventLoop>(new rmqio::AsioEventLoop()),
        options);

    rmqt::Result<rmqp::Connection> result =
        context
            .createNewConnection("LIBRMQ.ONLYCONNECT",
                                 params.endpoint(),
                                 params.credentials(),
                                 "onlyconnect")
            .blockResult();

    if (!result) {
        BALL_LOG_ERROR << "couldn't create connection: " << result.error();
        return 1;
    }

    BALL_LOG_INFO << "Connected";

    if (exitDelay) {
        BALL_LOG_INFO << "Waiting " << exitDelay << "s";
        bslmt::ThreadUtil::microSleep(0,
                                      exitDelay); // sleep for exitDelay seconds
        BALL_LOG_INFO << "Closing Connection";
        result.value()->close();
        return 0;
    }

    while (1) {
        bslmt::ThreadUtil::microSleep(0, 5); // sleep 5s
    }
}
