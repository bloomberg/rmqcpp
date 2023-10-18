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

#include <rmqintegration_testparameters.h>

#include <rmqa_connectionstring.h>
#include <rmqa_rabbitcontext.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_mutualsecurityparameters.h>
#include <rmqt_plaincredentials.h>

#include <balcl_commandline.h>
#include <ball_fileobserver.h>
#include <ball_log.h>
#include <ball_severityutil.h>
#include <ball_streamobserver.h>
#include <bdlb_guidutil.h>
#include <bsl_iostream.h>
#include <bsl_stdexcept.h>

namespace BloombergLP {
namespace rmqintegration {
namespace {

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
} // namespace

TestParameters::TestParameters(const bsl::string& moduleName)
: logLevel("INFO")
, uri("rabbit")
, port(5672)
, tlsPort(5671)
, vhost("rmq-lib")
, queueName(moduleName.substr(moduleName.find_last_of("/") + 1) + "-" +
            bdlb::GuidUtil::guidToString(bdlb::GuidUtil::generate()))
, consumerTag(rmqt::ConsumerConfig::generateConsumerTag())
, connectionName(moduleName)
, caCert()
, clientCert()
, clientKey()
, d_specTable()
, d_connectionCount()
{
    balcl::OptionInfo DEFAULT_CLIS[] = {
        {
            "h|uri",
            "uri",
            "RabbitMQ URI to connect to",
            balcl::TypeInfo(&uri),
            balcl::OccurrenceInfo(uri),
        },
        {
            "v|vhost",
            "vhost",
            "The vhost to connect to",
            balcl::TypeInfo(&vhost),
            balcl::OccurrenceInfo(vhost),
        },
        {
            "q|queue",
            "queue",
            "The queue to consume from",
            balcl::TypeInfo(&queueName),
            balcl::OccurrenceInfo(queueName),
        },
        {
            "t|tag",
            "consumertag",
            "The consumer tag to use",
            balcl::TypeInfo(&consumerTag),
            balcl::OccurrenceInfo(consumerTag),
        },
        {
            "c|connection",
            "connectionname",
            "The connection name to use",
            balcl::TypeInfo(&connectionName),
            balcl::OccurrenceInfo(connectionName),
        },
        {
            "o|logLevel",
            "logLevel",
            "Emit trace logs based on logLevel (OFF | FATAL | ERROR | WARN | "
            "INFO | DEBUG | TRACE)",
            balcl::TypeInfo(&logLevel),
            balcl::OccurrenceInfo(logLevel),
        },
        {
            "tlsport",
            "tls-port",
            "RabbitMQ port to connect on when using TLS",
            balcl::TypeInfo(&tlsPort),
            balcl::OccurrenceInfo(tlsPort),
        },
        {
            "ca",
            "certificateauthority",
            "path to CA cert",
            balcl::TypeInfo(&caCert),
            balcl::OccurrenceInfo(caCert),
        },
        {
            "cert",
            "clientCert",
            "path to client cert",
            balcl::TypeInfo(&clientCert),
            balcl::OccurrenceInfo(clientCert),
        },
        {
            "key",
            "clientKey",
            "path to client key",
            balcl::TypeInfo(&clientKey),
            balcl::OccurrenceInfo(clientKey),
        },
    };
    for (size_t i = 0; i < (sizeof(DEFAULT_CLIS) / sizeof(DEFAULT_CLIS[0]));
         ++i) {
        d_specTable.push_back(DEFAULT_CLIS[i]);
    }
}

rmqt::VHostInfo TestParameters::vhostInfo()
{
    bsl::optional<rmqt::VHostInfo> vhostInfo =
        rmqa::ConnectionString::parse(uri);
    if (!vhostInfo) {
        throw bsl::runtime_error("Failed to parse connection string: " + uri);
    }
    return *vhostInfo;
}

bsl::shared_ptr<rmqa::VHost>
TestParameters::createConnection(rmqa::RabbitContext& rabbit, int* id)
{
    return rabbit.createVHostConnection(
        connectionName + (id ? bsl::to_string(*id) : bsl::string()),
        this->vhostInfo());
}

bool TestParameters::parseAndConfigure(int argc, char* argv[])
{

    balcl::CommandLine cmdLine(d_specTable.begin(), d_specTable.size());

    // parse command-line options; if failure, print usage
    if (cmdLine.parse(argc, argv)) {
        cmdLine.printUsage();
        return false;
    }

    configLog(logLevel.c_str());

    return true;
}

} // namespace rmqintegration
} // namespace BloombergLP
