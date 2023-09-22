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

#include <rmqt_future.h>

#include <balcl_commandline.h>
#include <ball_log.h>
#include <bslmt_threadutil.h>

#include <bsl_memory.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.EXITCONSUMER")
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

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    bsl::string routingKey = "";
    int sleepPeriod        = 5;

    balcl::OptionInfo extraArgs[] = {
        {
            "r|routingKey",
            "routingKey",
            "The routing key for publishing messages",
            balcl::TypeInfo(&routingKey),
            balcl::OccurrenceInfo(routingKey),
        },
        {
            "w|sleepPeriod",
            "sleepPeriod",
            "How many seconds to sleep before exiting the task",
            balcl::TypeInfo(&sleepPeriod),
            balcl::OccurrenceInfo(sleepPeriod),
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    rmqa::RabbitContext rabbit;

    bsl::shared_ptr<rmqa::VHost> connection = params.createConnection(rabbit);

    rmqa::Topology topology;
    rmqt::QueueHandle queue = topology.addQueue(params.queueName);

    const bsl::string consumerTag         = "sample-consumer";
    const uint16_t prefetchCount          = 1;
    rmqt::Future<rmqa::Consumer> consumer = connection->createConsumerAsync(
        topology, queue, MessageConsumer(), consumerTag, prefetchCount);

    bslmt::ThreadUtil::microSleep(0, sleepPeriod);

    return 0;
}
