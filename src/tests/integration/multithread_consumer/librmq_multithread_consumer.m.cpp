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

#include <balcl_commandline.h>
#include <ball_log.h>
#include <bdlf_bind.h>
#include <bsl_memory.h>
#include <bsl_vector.h>
#include <bslmt_threadgroup.h>
#include <bslmt_threadutil.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.MULTITHREAD.CONSUMER")
}

// Consumer callback
class MessageConsumer {
  public:
    void operator()(rmqp::MessageGuard& guard)
    {
        BALL_LOG_INFO << "Received message: " << guard.message() << " Content: "
                      << bsl::string((const char*)guard.message().payload(),
                                     guard.message().payloadSize());
        guard.ack();
    }
};

void runConsumingTest(bsl::shared_ptr<rmqa::VHost> connection,
                      bsl::string queueName,
                      bsl::string consumerTag,
                      int prefetchCount)
{
    rmqa::Topology topology;
    rmqt::QueueHandle queue = topology.addQueue(queueName);

    rmqt::ConsumerConfig config;
    config.setConsumerTag(consumerTag).setPrefetchCount(prefetchCount);

    rmqt::Result<rmqa::Consumer> consumer =
        connection->createConsumer(topology, queue, MessageConsumer(), config);
    (void)consumer;

    while (1) {
        bslmt::ThreadUtil::microSleep(0, 5); // sleep 5s
    }
}

int main(int argc, char* argv[])
{

    rmqintegration::TestParameters params(__FILE__);

    int prefetch            = 100;
    int numberOfConsumers   = 5;
    int numberOfConnections = 2;

    balcl::OptionInfo extraArgs[] = {
        {
            "l|prefetch",
            "prefetch",
            "Prefetch count",
            balcl::TypeInfo(&prefetch),
            balcl::OccurrenceInfo(prefetch),
        },
        {
            "i|numberOfConsumers",
            "numberOfConsumers",
            "Number of consumers per connection",
            balcl::TypeInfo(&numberOfConsumers),
            balcl::OccurrenceInfo(numberOfConsumers),
        },
        {
            "j|numberOfConnections",
            "numberOfConnections",
            "Number of connections",
            balcl::TypeInfo(&numberOfConnections),
            balcl::OccurrenceInfo(numberOfConnections),
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    rmqa::RabbitContext rabbit;
    bsl::vector<bsl::shared_ptr<rmqa::VHost> > connections;
    for (int j = 0; j < numberOfConnections; ++j) {
        connections.push_back(params.createConnection(rabbit, &j));
    }

    bslmt::ThreadGroup threadGroup;
    for (int i = 0; i < numberOfConsumers; ++i) {
        for (int j = 0; j < numberOfConnections; ++j) {
            threadGroup.addThread(
                bdlf::BindUtil::bind(&runConsumingTest,
                                     connections[j],
                                     params.queueName,
                                     params.consumerTag + bsl::to_string(i) +
                                         "_" + bsl::to_string(j),
                                     prefetch));
        }
    }

    threadGroup.joinAll();
}
