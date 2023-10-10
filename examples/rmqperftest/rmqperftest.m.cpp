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

// An rmqcpp implementation of the interface provided by the RabbitMQ team's
// perf test tool https://github.com/rabbitmq/rabbitmq-perf-test

#include <rmqperftest_args.h>
#include <rmqperftest_runner.h>

#include <balcl_commandline.h>
#include <ball_log.h>
#include <ball_loggermanager.h>
#include <ball_loggermanagerconfiguration.h>
#include <ball_severityutil.h>
#include <ball_streamobserver.h>

#include <bslma_managedptr.h>
#include <bslmt_threadutil.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_string.h>

#include <cstdlib>

using namespace BloombergLP;
using namespace BloombergLP::rmqperftest;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQPFERTEST")

} // namespace

void configLog(const bsl::string& logLevel)
{
    static ball::LoggerManagerConfiguration configuration;
    static bsl::shared_ptr<ball::StreamObserver> loggingObserver =
        bsl::make_shared<ball::StreamObserver>(&bsl::cout);

    ball::Severity::Level level;
    if (ball::SeverityUtil::fromAsciiCaseless(&level, logLevel.c_str())) {
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
    PerfTestArgs args;

    bsl::string logLevel = "INFO";
    char* envLogLevel    = std::getenv("PERF_TEST_LOG");
    if (envLogLevel) {
        logLevel = envLogLevel;
    }

    char* envShuffleConnectionEndpoints =
        std::getenv("RMQPERFTEST_SHUFFLE_CONNECTION_ENDPOINTS");
    if (envShuffleConnectionEndpoints) {
        args.shuffleConnectionEndpoints = true;
    }

    balcl::OptionInfo specTable[] = {
        {
            "d|id",
            "id",
            "Test ID",
            balcl::TypeInfo(&args.testId),
            balcl::OccurrenceInfo(args.testId),
        },
        {
            "h|uri",
            "uri",
            "Connection URI",
            balcl::TypeInfo(&args.uri),
            balcl::OccurrenceInfo(args.uri),
        },
        {
            "z|time",
            "time",
            "Run duration in seconds (0==unlimited)",
            balcl::TypeInfo(&args.testRunDuration),
            balcl::OccurrenceInfo(args.testRunDuration),
        },
        {
            "auto-delete",
            "auto-delete",
            "If queue should not be auto-deleted use this flag and pass "
            "'false'. "
            "Otherwise queues are auto-delete by default.",
            balcl::TypeInfo(&args.topology.autoDelete),
            balcl::OccurrenceInfo(args.topology.autoDelete),
        },
        {
            "e|exchange",
            "exchange",
            "Exchange name",
            balcl::TypeInfo(&args.topology.exchangeName),
            balcl::OccurrenceInfo(args.topology.exchangeName),
        },
        {
            "t|type",
            "type",
            "Exchange type.",
            balcl::TypeInfo(&args.topology.exchangeType),
            balcl::OccurrenceInfo(args.topology.exchangeType),
        },
        {
            "u|queue",
            "queue",
            "Queue name",
            balcl::TypeInfo(&args.topology.queueName),
            balcl::OccurrenceInfo(args.topology.queueName),
        },
        {
            "queue-pattern",
            "queue-pattern",
            "Queue name pattern for creating queues in sequence. Use `--queue` "
            "if only a single queue is needed.",
            balcl::TypeInfo(&args.topology.queuePattern),
            balcl::OccurrenceInfo(args.topology.queuePattern),
        },
        {
            "queue-pattern-from",
            "queue-pattern-from",
            "Queue name pattern range start (inclusive). For use with "
            "`--queue-pattern`. Will create/use queues with the format "
            "<queue-pattern>-#",
            balcl::TypeInfo(&args.topology.queuePatternStartNum),
            balcl::OccurrenceInfo(args.topology.queuePatternStartNum),
        },
        {
            "queue-pattern-to",
            "queue-pattern-to",
            "Queue name pattern range end (inclusive).",
            balcl::TypeInfo(&args.topology.queuePatternEndNum),
            balcl::OccurrenceInfo(args.topology.queuePatternEndNum),
        },
        {
            "c|confirm",
            "confirms",
            "Max outstanding confirms/unconfirmed publishes",
            balcl::TypeInfo(&args.producer.maxOutstandingConfirms),
            balcl::OccurrenceInfo(args.producer.maxOutstandingConfirms),
        },
        {
            "C|pmessages",
            "pmessages",
            "Producer Message Count. Default is unlimited",
            balcl::TypeInfo(&args.producer.producerMessageCount),
            balcl::OccurrenceInfo(args.producer.producerMessageCount),
        },
        {
            "k|routing-key",
            "routing-key",
            "Routing Key",
            balcl::TypeInfo(&args.producer.routingKey),
            balcl::OccurrenceInfo(args.producer.routingKey),
        },
        {
            "P|publishing-interval",
            "publishing-interval",
            "Publishing interval in seconds (opposite of producer rate limit)",
            balcl::TypeInfo(&args.producer.publishingInterval),
            balcl::OccurrenceInfo(args.producer.publishingInterval),
        },
        {
            "s|size",
            "size",
            "Message size in bytes",
            balcl::TypeInfo(&args.producer.messageSize),
            balcl::OccurrenceInfo(args.producer.messageSize),
        },
        {
            "D|cmessages",
            "cmessages",
            "Consumer Message Count. Default is unlimited",
            balcl::TypeInfo(&args.consumer.consumerMessageCount),
            balcl::OccurrenceInfo(args.consumer.consumerMessageCount),
        },
        {
            "q|qos",
            "qos",
            "Consumer qos / consumer prefetch limit",
            balcl::TypeInfo(&args.consumer.prefetch),
            balcl::OccurrenceInfo(args.consumer.prefetch),
        },
        {
            "R|consumer-rate",
            "consumer-rate",
            "Consumer Rate Limit",
            balcl::TypeInfo(&args.consumer.consumerRateLimit),
            balcl::OccurrenceInfo(args.consumer.consumerRateLimit),
        },
        {
            "y|consumers",
            "consumers",
            "Consumer Count",
            balcl::TypeInfo(&args.consumer.numConsumers),
            balcl::OccurrenceInfo(args.consumer.numConsumers),
        },
        {
            "x|producers",
            "producers",
            "Producer Count",
            balcl::TypeInfo(&args.producer.numProducers),
            balcl::OccurrenceInfo(args.producer.numProducers),
        },
        {
            "consumer-args",
            "consumer-args",
            "Consumer arguments as a list of comma separated key/value pairs "
            "like 'x-priority=1', ...'",
            balcl::TypeInfo(&args.consumer.consumerArgs),
            balcl::OccurrenceInfo(args.consumer.consumerArgs),
        },
        {
            "f|flag",
            "flag",
            "Message Flag. Only 'persistent' is supported. All messages are "
            "sent with mandatory. Any values other than persistent are "
            "ignored.",
            balcl::TypeInfo(&args.producer.messageFlag),
            balcl::OccurrenceInfo(args.producer.messageFlag),
        },
        {
            "log-level",
            "log-level",
            "Log Level",
            balcl::TypeInfo(&logLevel),
            balcl::OccurrenceInfo(logLevel),
        },
    };
    balcl::CommandLine cmdLine(specTable);

    // parse command-line options; if failure, print usage
    if (cmdLine.parse(argc, argv)) {
        cmdLine.printUsage();
        return 1;
    }

    configLog(logLevel);

    return rmqperftest::Runner::run(args);
}
