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

#include <rmqperftest_consumerargs.h>
#include <rmqperftest_runner.h>

#include <rmqa_connectionstring.h>
#include <rmqa_consumer.h>
#include <rmqa_producer.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_vhost.h>
#include <rmqt_exchange.h>
#include <rmqt_vhostinfo.h>

#include <bsl_algorithm.h>
#include <bsl_cstdint.h>
#include <bsl_iostream.h>
#include <bsl_iterator.h>
#include <bsl_list.h>
#include <bsl_memory.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

#include <unistd.h>

#include <algorithm>

namespace BloombergLP {
namespace rmqperftest {
namespace {
class ConsumerCallback {
  public:
    ConsumerCallback(const ConsumerArgs& args,
                     const bsl::shared_ptr<bsls::AtomicBool>& finished)
    : d_args(args)
    , d_finished(finished)
    , d_remainingCount(bsl::make_shared<bsls::AtomicUint64>())
    , d_limitCount()
    {
        if (d_args.consumerMessageCount > 0) {
            d_limitCount      = true;
            *d_remainingCount = d_args.consumerMessageCount;
        }
    }

    void operator()(rmqp::MessageGuard& guard)
    {
        if (d_limitCount) {
            if (*d_remainingCount <= 0) {
                *d_finished = true;
                return;
            }
            else {
                *d_remainingCount -= 1;
            }
        }
        guard.ack();
    }

  private:
    ConsumerArgs d_args;
    bsl::shared_ptr<bsls::AtomicBool> d_finished;
    bsl::shared_ptr<bsls::AtomicUint64> d_remainingCount;
    bool d_limitCount;
};

class ConfirmCallback {
  public:
    ConfirmCallback() {}

    void operator()(const rmqt::Message&,
                    const bsl::string&,
                    const rmqt::ConfirmResponse&){};
};

bsl::string_view queueNameOrRoutingKey(bsl::string_view routingKey,
                                       bsl::string_view queueName)
{
    if (routingKey.empty()) {
        return queueName;
    }
    else {
        return routingKey;
    }
}

} // namespace

int Runner::run(const PerfTestArgs& args)
{
    rmqa::RabbitContextOptions options;
    options.setShuffleConnectionEndpoints(args.shuffleConnectionEndpoints);
    rmqa::RabbitContext rabbit(options);

    bsl::optional<rmqt::VHostInfo> vhostInfo =
        rmqa::ConnectionString::parse(args.uri);

    if (!vhostInfo) {
        bsl::cerr << "Failed to parse connection string: " << args.uri << "\n";
        return 1;
    }

    bsl::vector<bsl::shared_ptr<rmqa::VHost> > connections;

    for (int i = 0;
         i < std::max(args.producer.numProducers, args.consumer.numConsumers);
         i++) {
        connections.push_back(
            rabbit.createVHostConnection(args.testId, *vhostInfo));

        if (!connections[i]) {
            bsl::cerr << "Failed to create VHost " << i << "\n";
            return 1;
        }
    }

    // apply topology config
    rmqa::Topology topology;

    bsl::vector<rmqt::QueueHandle> v_queues;

    rmqt::AutoDelete::Value autoDelete = args.topology.autoDelete == "false"
                                             ? rmqt::AutoDelete::OFF
                                             : rmqt::AutoDelete::ON;
    // Persistent Message Flag implies durable in the java perf test tool
    rmqt::Durable::Value durable = args.producer.messageFlag == "persistent"
                                       ? rmqt::Durable::ON
                                       : rmqt::Durable::OFF;

    if (args.topology.queuePattern != "") {
        if (args.topology.queuePatternStartNum <=
            args.topology.queuePatternEndNum) {
            for (int i = args.topology.queuePatternStartNum;
                 i < args.topology.queuePatternEndNum + 1;
                 i++) {
                v_queues.push_back(topology.addQueue(
                    args.topology.queuePattern + "-" + bsl::to_string(i),
                    autoDelete,
                    durable));
            }
        }
        else {
            bsl::cerr << "`--queue-pattern-from` is greater than "
                         "`--queue-pattern-to`, "
                      << "please fix the queue pattern range\n";
            return 1;
        }
    }
    else {
        v_queues.push_back(
            topology.addQueue(args.topology.queueName, autoDelete, durable));
    }

    rmqt::ExchangeHandle exch =
        topology.addExchange(args.topology.exchangeName,
                             rmqt::ExchangeType(args.topology.exchangeType));

    if (args.topology.exchangeName != "") {
        for (size_t i = 0; i < v_queues.size(); i++) {
            // Use routingKey if specified, otherwise use the queue name as
            // routing key
            bsl::string_view routingKey = queueNameOrRoutingKey(
                args.producer.routingKey, v_queues[i].lock()->name());

            topology.bind(exch, v_queues[i], bsl::string(routingKey));
        }
    }

    // Create producer
    typedef bsl::pair<bsl::shared_ptr<rmqa::Producer>, bsl::string>
        ProducerRoutingKeyPair;
    bsl::vector<ProducerRoutingKeyPair> producers;
    {
        bsl::vector<rmqt::QueueHandle>::const_iterator queues;
        queues = v_queues.begin();
        for (int i = 0; i < args.producer.numProducers; i++) {
            rmqt::Result<rmqa::Producer> producerResult =
                connections[i]->createProducer(
                    topology, exch, args.producer.maxOutstandingConfirms);

            if (!producerResult) {
                bsl::cerr << "Failed to create producer: " << i << "\n";
                return 1;
            }

            producers.push_back(bsl::make_pair(
                producerResult.value(),
                bsl::string(queueNameOrRoutingKey(args.producer.routingKey,
                                                  queues->lock()->name()))));

            queues++;
            if (queues == v_queues.end()) {
                queues = v_queues.begin();
            }
        }
    }

    // Pair of <consumer, consumerFinished flag>
    typedef bsl::pair<bsl::shared_ptr<rmqa::Consumer>,
                      bsl::shared_ptr<bsls::AtomicBool> >
        ConsumerAndExitFlag;
    bsl::list<ConsumerAndExitFlag> consumers;

    // create queues iterator to handle the case where there are more consumers
    // than queues
    bsl::vector<rmqt::QueueHandle>::const_iterator queues;
    queues = v_queues.begin();

    for (int i = 0; i < args.consumer.numConsumers; i++) {
        rmqt::ConsumerConfig consumerConfig;
        consumerConfig.setConsumerTag(args.testId + " consumer " +
                                      bsl::to_string(i));
        consumerConfig.setPrefetchCount(args.consumer.prefetch);
        bsl::optional<long long> priority =
            findPriorityValue(args.consumer.consumerArgs);
        if (priority) {
            bsl::cout << "Setting Consumer priority to " << priority.value()
                      << bsl::endl;
            consumerConfig.setConsumerPriority(priority.value());
        }

        bsl::shared_ptr<bsls::AtomicBool> consumerFinishedFlag =
            bsl::make_shared<bsls::AtomicBool>(false);

        rmqt::Result<rmqa::Consumer> consumerResult =
            connections[i]->createConsumer(
                topology,
                *queues,
                ConsumerCallback(args.consumer, consumerFinishedFlag),
                consumerConfig);

        if (!consumerResult) {
            bsl::cerr << "Failed to create consumer\n";
            return 1;
        }

        consumers.push_back(
            bsl::make_pair(consumerResult.value(), consumerFinishedFlag));

        // increment the queues pointer or reset it. This cycles through the
        // QueueHandles created in v_queues
        queues++;
        if (queues == v_queues.end()) {
            queues = v_queues.begin();
        }
    }

    const bsls::TimeInterval startTime = bsls::SystemTime::nowMonotonicClock();

    bsls::TimeInterval nextPublish =
        startTime + bsls::TimeInterval(args.producer.publishingInterval, 0);

    size_t producerMessagesSent = 0;

    while (producers.size() > 0 || consumers.size() > 0) {
        const bsls::TimeInterval currentTime =
            bsls::SystemTime::nowMonotonicClock();

        const double seconds = (currentTime - startTime).totalSeconds();
        if (args.testRunDuration > 0 && seconds > args.testRunDuration) {
            break;
        }

        for (bsl::list<ConsumerAndExitFlag>::iterator it = consumers.begin();
             it != consumers.end();) {
            if (*(it->second)) {
                bsl::cout << "Consumer finished, exiting consumer\n";
                it = consumers.erase(it);
            }
            else {
                it++;
            }
        }

        // Sleep until the next publish
        const int64_t usSleep = (nextPublish - currentTime).totalMicroseconds();
        usleep(std::max(usSleep, (int64_t)0));

        nextPublish = nextPublish +
                      bsls::TimeInterval(args.producer.publishingInterval, 0);

        if (producers.size() > 0) {
            rmqt::Message msg(bsl::make_shared<bsl::vector<uint8_t> >(
                args.producer.messageSize, 'a'));

            if (args.producer.messageFlag == "persistent") {
                msg.updateDeliveryMode(rmqt::DeliveryMode::PERSISTENT);
            }
            else {
                msg.updateDeliveryMode(rmqt::DeliveryMode::NON_PERSISTENT);
            }

            for (bsl::vector<ProducerRoutingKeyPair>::iterator it =
                     producers.begin();
                 it != producers.end();
                 ++it) {
                it->first->send(msg, it->second, ConfirmCallback());
            }

            producerMessagesSent += 1;

            if ((int)producerMessagesSent ==
                args.producer.producerMessageCount) {
                bsl::cout << producers.size()
                          << " producers finished, exiting producers"
                          << bsl::endl;
                producers.clear();
            }
        }
    }

    bsl::cout
        << "Test finished in "
        << (bsls::SystemTime::nowMonotonicClock() - startTime).totalSeconds()
        << " seconds" << bsl::endl;

    return 0;
}

} // namespace rmqperftest
} // namespace BloombergLP
