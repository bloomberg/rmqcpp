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

#include <rmqa_producer.h>
#include <rmqa_rabbitcontext.h>

#include <rmqt_plaincredentials.h>

#include <bslmt_semaphore.h>
#include <bslmt_threadutil.h>

#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.MULTICONNECT")
}

void onConfirmUpdate(const rmqt::Message& message,
                     const bsl::string& routingKey,
                     const rmqt::ConfirmResponse& confirmResponse)
{
    BALL_LOG_INFO << confirmResponse << " for " << message
                  << " routing-key: " << routingKey;
}

class MessageConsumer {
  public:
    MessageConsumer(size_t exitAfter,
                    const bsl::shared_ptr<bslmt::Semaphore>& exitSemaphore)
    : d_exitAfter(exitAfter)
    , d_counter(bsl::make_shared<bsls::AtomicInt>())
    , d_exitSemaphore(exitSemaphore)
    {
        BALL_LOG_INFO << "MessageConsumer, exitAfter = " << d_exitAfter;
    }

    void operator()(rmqp::MessageGuard& guard)
    {
        size_t counter = d_counter->add(1);

        if (d_exitAfter != 0 && counter > d_exitAfter) {
            return;
        }

        BALL_LOG_INFO << "Received message: " << guard.message()
                      << " Counter: " << counter << " Content: "
                      << bsl::string((const char*)guard.message().payload(),
                                     guard.message().payloadSize());

        BALL_LOG_INFO << "Sending ACK...";
        guard.ack();

        if (d_exitAfter != 0 && counter == d_exitAfter) {
            BALL_LOG_INFO << "Consumed " << counter << " messages, exiting...";
            d_exitSemaphore->post();
            return;
        }
    }

  private:
    size_t d_exitAfter;
    bsl::shared_ptr<bsls::AtomicInt> d_counter;
    bsl::shared_ptr<bslmt::Semaphore> d_exitSemaphore;
};

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    int connectionCount           = 2;
    int messagesPublished         = 100;
    bool exitOnConnect            = false;
    balcl::OptionInfo extraArgs[] = {
        {
            "n|connectioncount",
            "connectioncount",
            "The connections to create",
            balcl::TypeInfo(&connectionCount),
            balcl::OccurrenceInfo(connectionCount),
        },
        {
            "m|messagecount",
            "messagecount",
            "Number of messages to send/receive on each connection",
            balcl::TypeInfo(&messagesPublished),
            balcl::OccurrenceInfo(messagesPublished),
        },
        {
            "e|exit",
            "exit",
            "Exit after all connections are complete",
            balcl::TypeInfo(&exitOnConnect),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    rmqa::Topology topology;
    rmqt::QueueHandle queue = topology.addQueue(params.queueName);
    rmqt::ExchangeHandle exchange =
        topology.addExchange("exchange", rmqt::ExchangeType::DIRECT);
    topology.bind(exchange, queue, params.queueName);

    rmqa::RabbitContext rabbit;
    bsl::vector<bsl::shared_ptr<rmqa::VHost> > connections;
    bsl::vector<bsl::shared_ptr<rmqa::Producer> > producers;
    bsl::vector<bsl::shared_ptr<rmqa::Consumer> > consumers;
    bsl::shared_ptr<bslmt::Semaphore> exitSemaphore =
        bsl::make_shared<bslmt::Semaphore>();

    for (int i = 0; i < connectionCount; ++i) {
        BALL_LOG_INFO << "Creating producer and consumer " << i;
        bsl::shared_ptr<rmqa::VHost> connection =
            params.createConnection(rabbit, &i);
        connections.push_back(connection);
        BALL_LOG_INFO << "create producer " << i;
        bsl::shared_ptr<rmqa::Producer> producer =
            connection->createProducer(topology, exchange, 10).value();
        producers.push_back(producer);
        BALL_LOG_INFO << "producer " << i;

        bsl::shared_ptr<rmqa::Consumer> consumer =
            connection
                ->createConsumer(
                    topology,
                    queue,
                    MessageConsumer(messagesPublished, exitSemaphore))
                .value();
        consumers.push_back(consumer);
        BALL_LOG_INFO << "consumer " << i;
    }
    for (int i = 0; i < messagesPublished; ++i) {
        for (int j = 0; j < connectionCount; ++j) {
            bsl::string text = "rmqcpp message #" + bsl::to_string(i) +
                               " from #" + bsl::to_string(j);
            rmqt::Message message(bsl::make_shared<bsl::vector<uint8_t> >(
                text.cbegin(), text.cend()));

            BALL_LOG_INFO << "About to call producer->send.";

            producers[j]->send(message, params.queueName, &onConfirmUpdate);
        }
    }

    for (int i = 0; i < connectionCount; ++i) {
        exitSemaphore->wait();
    }

    if (!exitOnConnect) {
        while (true) {
            bslmt::ThreadUtil::microSleep(0, 5); // sleep 5s
        }
    }
}
