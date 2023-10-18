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
#include <rmqtestutil_timedmetric.h>

#include <rmqa_consumer.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_vhost.h>
#include <rmqp_consumertracing.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_plaincredentials.h>

#include <balcl_commandline.h>

#include <ball_log.h>
#include <ball_severityutil.h>
#include <ball_streamobserver.h>

#include <bslmt_semaphore.h>
#include <bslmt_threadutil.h>
#include <bsls_atomic.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_string.h>

using namespace BloombergLP;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.CONSUMEMESSAGES")

struct Tracing : public rmqp::ConsumerTracing {
    struct Context : public rmqp::ConsumerTracing::Context {
        Context(const rmqt::Message& msg)
        {
            BALL_LOG_DEBUG << "Context Created: " << msg.messageId();
        }

        ~Context() { BALL_LOG_DEBUG << "Context Destroyed"; }
    };

    bsl::shared_ptr<rmqp::ConsumerTracing::Context>
    create(const rmqp::MessageGuard& messageGuard,
           const bsl::string&,
           const bsl::shared_ptr<const rmqt::Endpoint>&) const
    {
        return bsl::make_shared<Context>(messageGuard.message());
    }
};
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

class MessageConsumer {
  public:
    MessageConsumer(bool readFromStdin,
                    size_t exitAfter,
                    bool requeue,
                    bsls::AtomicInt& counter,
                    const bsl::shared_ptr<bslmt::Semaphore>& exitSemaphore,
                    int sleepSeconds)
    : d_readFromStdin(readFromStdin)
    , d_exitAfter(exitAfter)
    , d_requeue(requeue)
    , d_counter(counter)
    , d_exitSemaphore(exitSemaphore)
    , d_sleepSeconds(sleepSeconds)
    {
        BALL_LOG_INFO << "MessageConsumer, exitAfter = " << d_exitAfter;
    }

    void operator()(rmqp::MessageGuard& guard)
    {
        // Atomically increment d_counter and store its value in counter for
        // further use in this invocation
        size_t counter = d_counter.add(1);

        bslmt::ThreadUtil::microSleep(0, d_sleepSeconds);

        if (d_exitAfter != 0 && counter > d_exitAfter) {
            d_counter.subtract(1);
            return;
        }

        BALL_LOG_TRACE << "Received message: " << guard.message()
                       << " Counter: " << counter << " Content: "
                       << bsl::string((const char*)guard.message().payload(),
                                      guard.message().payloadSize());

        if (d_readFromStdin) {
            bsl::string input;
            bsl::cin >> input;
            if (input == "A") {
                BALL_LOG_INFO << "Sending ACK...";
                guard.ack();
            }
            else if (input == "N") {
                BALL_LOG_INFO << "Sending NACK (reject)...";
                guard.nack(false);
            }
            else if (input == "R") {
                BALL_LOG_INFO << "Sending NACK (requeue)...";
                guard.nack(true);
            }
            else if (input == "I") {
                BALL_LOG_INFO << "IGNORING...";
                return;
            }
            else {
                BALL_LOG_ERROR << "Unsuported ack type (A/N/R)";
                guard.nack(true);
            }
        }
        else {
            if (d_requeue) {
                BALL_LOG_TRACE << "Sending REQUEUE...";
                guard.nack(true);
            }
            else {
                BALL_LOG_TRACE << "Sending ACK...";
                guard.ack();
            }
        }

        if (d_exitAfter != 0 && counter == d_exitAfter) {
            BALL_LOG_INFO << "Consumed " << counter << " messages, exiting...";
            d_exitSemaphore->post();
            return;
        }
    }

  private:
    bool d_readFromStdin;
    size_t d_exitAfter;
    bool d_requeue;
    bsls::AtomicInt& d_counter;
    bsl::shared_ptr<bslmt::Semaphore> d_exitSemaphore;
    int d_sleepSeconds;
};

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    bool tracing       = false;
    int exitAfter      = 0;
    int prefetch       = 1000;
    bool readFromStdin = false;
    bool requeue       = false;
    int messageTTL     = 0;
    int queueTTL       = 0;
    int sleepSeconds   = 0;

    balcl::OptionInfo extraArgs[] = {
        {
            "n|count",
            "count",
            "Exit after n consumed messages",
            balcl::TypeInfo(&exitAfter),
            balcl::OccurrenceInfo(exitAfter),
        },
        {
            "r|requeue",
            "requeue",
            "Requeue received messages",
            balcl::TypeInfo(&requeue),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "d|tracing",
            "tracing",
            "trace consumed messages",
            balcl::TypeInfo(&tracing),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "l|prefetch",
            "prefetch",
            "Prefetch count",
            balcl::TypeInfo(&prefetch),
            balcl::OccurrenceInfo(prefetch),
        },
        {
            "stdin",
            "stdin",
            "Read from stdin",
            balcl::TypeInfo(&readFromStdin),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "k|messageTTL",
            "messageTTL",
            "Message TTL ",
            balcl::TypeInfo(&messageTTL),
            balcl::OccurrenceInfo(messageTTL),
        },
        {
            "x|expires",
            "queueTTL",
            "Queue TTL ",
            balcl::TypeInfo(&queueTTL),
            balcl::OccurrenceInfo(queueTTL),
        },
        {
            "sleep",
            "sleepSeconds",
            "Sleep for this many seconds before acking the consumed message",
            balcl::TypeInfo(&sleepSeconds),
            balcl::OccurrenceInfo(sleepSeconds),
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    bdlmt::ThreadPool consumerThreadPool(
        bslmt::ThreadAttributes(), 1, 1, 60000);
    consumerThreadPool.start();

    rmqa::RabbitContextOptions options;
    if (tracing) {
        options.setConsumerTracing(bsl::make_shared<Tracing>());
    }
    rmqa::RabbitContext rabbit(options);

    bsl::shared_ptr<rmqa::VHost> connection = params.createConnection(rabbit);

    rmqa::Topology topology;
    rmqt::FieldTable fieldTable;
    if (queueTTL > 0) {
        fieldTable["x-expires"] = rmqt::FieldValue(uint32_t(queueTTL));
    }
    if (messageTTL > 0) {
        fieldTable["x-message-ttl"] = rmqt::FieldValue(uint32_t(messageTTL));
    }
    rmqt::QueueHandle queue = topology.addQueue(
        params.queueName, rmqt::AutoDelete::OFF, rmqt::Durable::ON, fieldTable);

    bsl::shared_ptr<bslmt::Semaphore> exitSemaphore =
        bsl::make_shared<bslmt::Semaphore>();
    bsls::AtomicInt counter(0);
    rmqtestutil::TimedMetric<bsls::AtomicInt> timedMetric(counter);

    rmqt::Result<rmqa::Consumer> consumer =
        connection->createConsumer(topology,
                                   queue,
                                   MessageConsumer(readFromStdin,
                                                   exitAfter,
                                                   requeue,
                                                   counter,
                                                   exitSemaphore,
                                                   sleepSeconds),
                                   rmqt::ConsumerConfig()
                                       .setConsumerTag(params.consumerTag)
                                       .setPrefetchCount(prefetch)
                                       .setThreadpool(&consumerThreadPool));

    exitSemaphore->wait();
}
