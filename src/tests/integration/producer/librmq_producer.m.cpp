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

#include <rmqa_producer.h>
#include <rmqa_rabbitcontext.h>
#include <rmqa_rabbitcontextoptions.h>
#include <rmqa_vhost.h>
#include <rmqp_producertracing.h>
#include <rmqt_plaincredentials.h>
#include <rmqt_properties.h>
#include <rmqt_simpleendpoint.h>

#include <balcl_commandline.h>
#include <ball_log.h>
#include <bslmt_threadutil.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.PRODUCER")

struct Tracing : public rmqp::ProducerTracing {
    struct Context : public rmqp::ProducerTracing::Context {
        Context() { BALL_LOG_DEBUG << "Context Created"; }

        void response(const rmqt::ConfirmResponse& confirm)
        {
            BALL_LOG_DEBUG << "Context Completed: " << confirm;
        }
    };

    bsl::shared_ptr<rmqp::ProducerTracing::Context>
    createAndTag(rmqt::Properties*,
                 const bsl::string&,
                 const bsl::string&,
                 const bsl::shared_ptr<const rmqt::Endpoint>&) const
    {
        return bsl::make_shared<Tracing::Context>();
    }
};
} // namespace

void onConfirmUpdate(const rmqt::Message& message,
                     const bsl::string& routingKey,
                     const rmqt::ConfirmResponse& confirmResponse)
{
    BALL_LOG_DEBUG << confirmResponse << " for " << message
                   << " routing-key: " << routingKey;
}

void onError(const bsl::string& errorText, int errorCode)
{
    bsl::cout << "Error: " << errorText << "\tErrorCode: " << errorCode << "\n";
}

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    bsl::string routingKey     = "";
    int send_n                 = 0;
    int sleepPeriod            = 5;
    int maxOutstandingConfirms = 5;
    int messageSize            = 100;
    int messageTTL             = 0;
    int queueTTL               = 0;
    bool persistent            = false;
    bool tracing               = false;
    bool notMandatory          = false; // sorry for double negative :(

    balcl::OptionInfo extraArgs[] = {
        {
            "r|routingKey",
            "routingKey",
            "The routing key for publishing messages",
            balcl::TypeInfo(&routingKey),
            balcl::OccurrenceInfo(routingKey),
        },
        {
            "n|send",
            "send",
            "How many messages to send (0 to send indefinitely)",
            balcl::TypeInfo(&send_n),
            balcl::OccurrenceInfo(send_n),
        },
        {
            "w|sleepPeriod",
            "sleepPeriod",
            "How many seconds to sleep after sending each message",
            balcl::TypeInfo(&sleepPeriod),
            balcl::OccurrenceInfo(sleepPeriod),
        },
        {
            "l|maxOutstanding",
            "maxOutstanding",
            "Maximum number of outstanding message confirms before sending "
            "blocks",
            balcl::TypeInfo(&maxOutstandingConfirms),
            balcl::OccurrenceInfo(maxOutstandingConfirms),
        },
        {
            "m|messageSize",
            "messageSize",
            "Message size ",
            balcl::TypeInfo(&messageSize),
            balcl::OccurrenceInfo(messageSize),
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
            "e|persistent",
            "persistent",
            "Persistent Delivery Mode",
            balcl::TypeInfo(&persistent),
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
            "not-mandatory",
            "not-mandatory",
            "Disable Mandatory Flag",
            balcl::TypeInfo(&notMandatory),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    if (routingKey.empty()) {
        routingKey = params.queueName;
    }

    rmqt::ErrorCallback errorCb = &onError;
    rmqa::RabbitContextOptions rabbitOptions;
    rabbitOptions.setErrorCallback(errorCb);
    if (tracing) {
        rabbitOptions.setProducerTracing(bsl::make_shared<Tracing>());
    }
    rmqa::RabbitContext rabbit(rabbitOptions);

    bsl::shared_ptr<rmqa::VHost> connection = params.createConnection(rabbit);

    rmqa::Topology topology;
    rmqt::ExchangeHandle exch1 = topology.addExchange(
        "source-exchange", rmqt::ExchangeType::FANOUT, rmqt::AutoDelete::ON);
    rmqt::ExchangeHandle exch2 =
        topology.addExchange("destination-exchange",
                             rmqt::ExchangeType::DIRECT,
                             rmqt::AutoDelete::ON);
    topology.bind(exch1, exch2, "");
    rmqt::FieldTable fieldTable;
    if (queueTTL > 0) {
        fieldTable["x-expires"] = rmqt::FieldValue(uint32_t(queueTTL));
    }
    if (messageTTL > 0) {
        fieldTable["x-message-ttl"] = rmqt::FieldValue(uint32_t(messageTTL));
    }
    rmqt::QueueHandle queue = topology.addQueue(
        params.queueName, rmqt::AutoDelete::OFF, rmqt::Durable::ON, fieldTable);
    topology.bind(exch2, queue, params.queueName);

    rmqt::Result<rmqa::Producer> producerResult =
        connection->createProducer(topology, exch1, maxOutstandingConfirms);

    if (!producerResult) {
        BALL_LOG_ERROR << "Could not create producer: "
                       << producerResult.error();
        return 1;
    }

    const bsl::string messageText(messageSize, '*');

    bsl::shared_ptr<rmqa::Producer> producer = producerResult.value();

    size_t messagesSent = 0;
    rmqtestutil::TimedMetric<size_t> timedMetric(messagesSent);
    for (messagesSent = 0;
         send_n == 0 || messagesSent < static_cast<size_t>(send_n);
         ++messagesSent) {
        rmqt::Message message(bsl::make_shared<bsl::vector<uint8_t> >(
            messageText.cbegin(), messageText.cend()));

        if (!persistent) {
            message.updateDeliveryMode(rmqt::DeliveryMode::NON_PERSISTENT);
        }
        rmqt::Mandatory::Value mandatory =
            notMandatory ? rmqt::Mandatory::DISCARD_UNROUTABLE
                         : rmqt::Mandatory::RETURN_UNROUTABLE;

        BALL_LOG_DEBUG << "About to call producer->send.";

        rmqp::Producer::SendStatus status =
            producer->send(message, routingKey, mandatory, &onConfirmUpdate);

        if (status == rmqp::Producer::SENDING) {
            BALL_LOG_DEBUG << "Sending message: " << message;
        }
        else {
            BALL_LOG_ERROR << "Failed to send (rc: " << status
                           << ") message: " << message;
        }

        if (sleepPeriod > 0) {
            bslmt::ThreadUtil::microSleep(0, sleepPeriod);
        }
    }

    BALL_LOG_INFO
        << "Waiting for remaining broker confirmations for published messages";
    producer->waitForConfirms();
}