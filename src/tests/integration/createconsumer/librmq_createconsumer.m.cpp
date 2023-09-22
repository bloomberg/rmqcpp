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

#include <rmqa_consumer.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_vhost.h>

#include <rmqt_plaincredentials.h>

#include <balcl_commandline.h>
#include <bslmt_semaphore.h>
#include <bslmt_threadutil.h>

#include <ball_log.h>
#include <bsls_types.h>

#include <bsl_cstdint.h>
#include <bsl_memory.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.CREATECONSUMER")
}

// Consumer callback
class MessageConsumer {
  public:
    MessageConsumer(bslmt::Semaphore& semaphore)
    : d_semaphore(semaphore)
    {
    }

    void operator()(rmqp::MessageGuard& guard)
    {
        BALL_LOG_INFO << "Received message: " << guard.message() << " Content: "
                      << bsl::string((const char*)guard.message().payload(),
                                     guard.message().payloadSize());
        d_semaphore.post();
        guard.ack();
    }

    bslmt::Semaphore& d_semaphore;
};

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);
    int prefetchCount                   = 100;
    bool exclusiveConsumer              = false;
    bsls::Types::Int64 consumerPriority = 0;
    int stopConsumerAfterNMessages      = 0;

    balcl::OptionInfo extraArgs[] = {
        {
            "l|qos",
            "prefetch",
            "The prefetch count",
            balcl::TypeInfo(&prefetchCount),
            balcl::OccurrenceInfo(prefetchCount),
        },
        {
            "e|exclusive",
            "exclusive",
            "Declare the consumer as exclusive",
            balcl::TypeInfo(&exclusiveConsumer),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "consumerPriority",
            "consumerPriority",
            "Set a consumer priority",
            balcl::TypeInfo(&consumerPriority),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "stopConsumerAfterNMessages",
            "stopConsumerAfterNMessages",
            "Stop the consumer once it has been successfully created and "
            "processed the given number of messages, leaving only the "
            "connection still open.",
            balcl::TypeInfo(&stopConsumerAfterNMessages),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    rmqa::RabbitContext rabbit;

    bsl::shared_ptr<rmqa::VHost> connection = params.createConnection(rabbit);

    rmqa::Topology topology;
    rmqt::ExchangeHandle exch = topology.addExchange(
        "source-exchange", rmqt::ExchangeType::FANOUT, rmqt::AutoDelete::ON);
    rmqt::QueueHandle queue = topology.addQueue(params.queueName);
    topology.bind(exch, queue, "routing-key");

    rmqt::ConsumerConfig config;
    config.setConsumerTag(params.consumerTag)
        .setPrefetchCount(prefetchCount)
        .setConsumerPriority(consumerPriority);

    if (exclusiveConsumer) {
        config.setExclusiveFlag();
    }

    bsl::shared_ptr<rmqa::Consumer> consumerKeptAlive;

    {
        bslmt::Semaphore semaphore(0);
        MessageConsumer messageConsumer(semaphore);

        rmqt::Result<rmqa::Consumer> consumer = connection->createConsumer(
            topology, queue, messageConsumer, config);

        if (stopConsumerAfterNMessages != 0) {
            for (int i = 0; i < stopConsumerAfterNMessages; i++) {
                semaphore.wait();
            }
            BALL_LOG_INFO << "Consumer destruct";
        }
        else {
            consumerKeptAlive = consumer.value();
        }
    }

    while (1) {
        bslmt::ThreadUtil::microSleep(0, 5); // sleep 5s
    }
}
