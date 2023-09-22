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
#include <rmqa_producer.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_topology.h>
#include <rmqa_topologyupdate.h>
#include <rmqa_vhost.h>
#include <rmqt_result.h>

#include <balcl_commandline.h>
#include <ball_log.h>

#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.TOPOLOGY")
}

void onConfirmUpdate(const rmqt::Message& message,
                     const bsl::string& routingKey,
                     const rmqt::ConfirmResponse& confirmResponse)
{
    BALL_LOG_DEBUG << confirmResponse << " for " << message
                   << " routing-key: " << routingKey;
}

void onMessageConsume(rmqp::MessageGuard& guard) { guard.ack(); }

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    bsl::string exchangeName = "sample-exchange";
    bsl::string exchangeType = "direct";
    bool autoDeleteX         = false;
    bool durableX            = false;
    bool internalX           = false;
    bool autoDeleteQ         = false;
    bool durableQ            = false;
    bsl::string bindingKey   = "";
    bsl::string routingKey   = "";

    balcl::OptionInfo extraArgs[] = {
        {
            "x|exchange",
            "exchange",
            "The exchange to publish messages to",
            balcl::TypeInfo(&exchangeName),
            balcl::OccurrenceInfo(exchangeName),
        },
        {
            "y|exchangeType",
            "exchangeType",
            "Exchange type (direct | fanout | topic)",
            balcl::TypeInfo(&exchangeType),
            balcl::OccurrenceInfo(exchangeType),
        },
        {
            "autoDeleteX",
            "autoDeleteX",
            "Autodelete exchange",
            balcl::TypeInfo(&autoDeleteX),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "durableX",
            "durableX",
            "Durable exchange",
            balcl::TypeInfo(&durableX),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "internalX",
            "internalX",
            "Internal exchange",
            balcl::TypeInfo(&internalX),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "autoDeleteQ",
            "autoDeleteQ",
            "Autodelete queue",
            balcl::TypeInfo(&autoDeleteQ),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "durableQ",
            "durableQ",
            "Durable queue",
            balcl::TypeInfo(&durableQ),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "b|bindingKey",
            "bindingKey",
            "The binding key to bind queue to exchange",
            balcl::TypeInfo(&bindingKey),
            balcl::OccurrenceInfo(bindingKey),
        },
        {
            "r|routingKey",
            "routingKey",
            "The routing key for publishing messages",
            balcl::TypeInfo(&routingKey),
            balcl::OccurrenceInfo(routingKey),
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    if (routingKey.empty()) {
        routingKey = params.queueName;
    }

    if (bindingKey.empty()) {
        bindingKey = params.queueName;
    }

    rmqa::RabbitContext rabbit;

    bsl::shared_ptr<rmqa::VHost> connection = params.createConnection(rabbit);

    rmqa::Topology topology;
    rmqt::ExchangeHandle exch1 = topology.addExchange(
        "source-exchange", rmqt::ExchangeType::FANOUT, rmqt::AutoDelete::ON);
    rmqt::ExchangeHandle exch2 = topology.addExchange(
        exchangeName,
        rmqt::ExchangeType(exchangeType),
        autoDeleteX ? rmqt::AutoDelete::ON : rmqt::AutoDelete::OFF,
        durableX ? rmqt::Durable::ON : rmqt::Durable::OFF,
        internalX ? rmqt::Internal::YES : rmqt::Internal::NO);
    topology.bind(exch1, exch2, "");
    rmqt::QueueHandle queue = topology.addQueue(
        params.queueName,
        autoDeleteQ ? rmqt::AutoDelete::ON : rmqt::AutoDelete::OFF,
        durableQ ? rmqt::Durable::ON : rmqt::Durable::OFF);
    topology.bind(exch2, queue, routingKey);

    rmqt::Result<rmqa::Producer> producerResult =
        connection->createProducer(topology, exch1, 1);
    if (!producerResult) {
        BALL_LOG_ERROR << "Could not create producer: "
                       << producerResult.error();
        return 1;
    }
    bsl::shared_ptr<rmqa::Producer> producer = producerResult.value();
    const bsl::string contents               = "sample-message";
    rmqt::Message msg(bsl::make_shared<bsl::vector<uint8_t> >(contents.cbegin(),
                                                              contents.cend()));
    producer->send(msg, routingKey, &onConfirmUpdate);
    producer->waitForConfirms();

    rmqa::TopologyUpdate topologyUpdate;
    topologyUpdate.bind(exch1, queue, "RandomKey");
    topologyUpdate.unbind(exch1, queue, "RandomKey");
    topologyUpdate.bind(exch1, queue, "RandomKey");
    producer->updateTopology(topologyUpdate, bsls::TimeInterval(10));

    rmqt::QueueHandle queue2 = topology.addQueue(
        params.queueName + "2",
        autoDeleteQ ? rmqt::AutoDelete::ON : rmqt::AutoDelete::OFF,
        durableQ ? rmqt::Durable::ON : rmqt::Durable::OFF);
    rmqt::Result<rmqa::Consumer> consumerResults =
        connection->createConsumer(topology, queue2, onMessageConsume);
    if (!consumerResults) {
        BALL_LOG_ERROR << "Could not create consumer: "
                       << consumerResults.error();
        return 1;
    }
    bsl::shared_ptr<rmqa::Consumer> consumer = consumerResults.value();
    rmqa::TopologyUpdate topologyUpdate2;
    topologyUpdate2.bind(exch1, queue2, "RandomKey2");
    consumer->updateTopology(topologyUpdate2, bsls::TimeInterval(10));

    return 0;
}
