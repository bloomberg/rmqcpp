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
#include <rmqt_consumerconfig.h>

#include <balcl_commandline.h>

#include <ball_log.h>
#include <bslmt_latch.h>
#include <bsls_atomic.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_sstream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

using namespace BloombergLP;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.CONSUMEMESSAGES")
} // namespace

class MessageConsumer {
  public:
    MessageConsumer(bslmt::Latch& latch)
    : d_latch(latch)
    , d_counter()
    {
    }

    void operator()(rmqp::MessageGuard& guard)
    {
        bsl::string message =
            bsl::string((const char*)guard.message().payload(),
                        guard.message().payloadSize());
        BALL_LOG_INFO << "Received message: " << guard.message()
                      << " Counter: " << d_counter++ << " Content: " << message
                      << " Thread: " << bslmt::ThreadUtil::selfIdAsInt();

        guard.ack();
        if (message == "exit") {
            d_latch.arrive();
        }
    }

  private:
    bsl::string d_name;
    bslmt::Latch& d_latch;
    bsls::AtomicUint d_counter;
};

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    int prefetch     = 1000;
    int numConsumers = 1;
    int queueTTL     = 60000;

    balcl::OptionInfo extraArgs[] = {
        {
            "l|prefetch",
            "prefetch",
            "Prefetch count",
            balcl::TypeInfo(&prefetch),
            balcl::OccurrenceInfo(prefetch),
        },
        {
            "n|numConsumers",
            "numConsumers",
            "How many consumers to start",
            balcl::TypeInfo(&numConsumers),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "x|expires",
            "queueTTL",
            "Queue TTL ",
            balcl::TypeInfo(&queueTTL),
            balcl::OccurrenceInfo(queueTTL),
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    };
    bsl::cout << "Number of consumers: " << numConsumers << bsl::endl;
    rmqa::RabbitContext rabbit;

    bsl::shared_ptr<rmqa::VHost> connection = params.createConnection(rabbit);

    bslmt::Latch latch(numConsumers);
    bsl::vector<bsl::shared_ptr<rmqa::Consumer> > consumers;
    for (int i = 0; i < numConsumers; ++i) {
        rmqa::Topology topology;
        rmqt::FieldTable fieldTable;
        if (queueTTL > 0) {
            fieldTable["x-expires"] = rmqt::FieldValue(uint32_t(queueTTL));
        }
        bsl::string queueName = params.queueName + bsl::to_string(i);
        rmqt::ExchangeHandle exch =
            topology.addExchange("source-exchange",
                                 rmqt::ExchangeType::FANOUT,
                                 rmqt::AutoDelete::ON);

        rmqt::QueueHandle queue = topology.addQueue(
            queueName, rmqt::AutoDelete::OFF, rmqt::Durable::ON, fieldTable);
        topology.bind(exch, queue, "routing-key");
        consumers.push_back(connection
                                ->createConsumer(topology,
                                                 queue,
                                                 MessageConsumer(latch),
                                                 rmqt::ConsumerConfig()
                                                     .setPrefetchCount(prefetch)
                                                     .setConsumerTag(queueName))

                                .value());
    }

    int result =
        latch.timedWait(bsls::SystemTime::nowRealtimeClock().addSeconds(600));
    if (result) {
        BALL_LOG_ERROR << "Timed Out";
    }
    return result;
}
