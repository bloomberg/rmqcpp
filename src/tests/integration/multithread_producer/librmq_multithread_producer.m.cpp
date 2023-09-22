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
#include <rmqa_rabbitcontextoptions.h>
#include <rmqa_topology.h>
#include <rmqa_vhost.h>
#include <rmqt_result.h>

#include <balcl_commandline.h>
#include <ball_log.h>
#include <bdlf_bind.h>
#include <bslmt_threadgroup.h>
#include <bslmt_threadutil.h>
#include <bsls_atomic.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.MULTITHREAD.PRODUCERS")

bsls::AtomicInt sentCount;
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

void runPublishingTest(bsl::shared_ptr<rmqa::VHost> connection,
                       bsl::string queueName,
                       bsl::string threadName,
                       int maxOutstandingConfirms,
                       int messageSize,
                       int send_n,
                       int sleepPeriod)
{
    BALL_LOG_INFO << threadName << ": started";
    bsl::string exchangeName = "default-exchange";
    rmqa::Topology topology;
    rmqt::ExchangeHandle exchangeHandle = topology.addExchange(exchangeName);
    rmqt::QueueHandle queueHandle       = topology.addQueue(queueName);
    topology.bind(exchangeHandle, queueHandle, queueName);

    rmqt::Result<rmqa::Producer> producerCreationResult =
        connection->createProducer(
            topology, exchangeHandle, maxOutstandingConfirms);
    if (!producerCreationResult) {
        BALL_LOG_ERROR << "Failed to create producer "
                       << "[error=" << producerCreationResult.error() << "]";
        return;
    }

    const bsl::string messageText(messageSize, '*');
    bsl::shared_ptr<rmqa::Producer> producer = producerCreationResult.value();

    for (int messagesSent = 0; messagesSent < send_n; ++messagesSent) {
        rmqt::Message message(bsl::make_shared<bsl::vector<uint8_t> >(
            messageText.cbegin(), messageText.cend()));

        BALL_LOG_INFO << threadName << ": send " << messagesSent;

        rmqp::Producer::SendStatus status =
            producer->send(message, queueName, &onConfirmUpdate);

        if (status == rmqp::Producer::SENDING) {
            BALL_LOG_DEBUG << "Sending message: " << messagesSent << " "
                           << message;
            ++sentCount;
        }
        else {
            BALL_LOG_ERROR << "Failed to send (rc: " << status
                           << ") message: " << message;
        }

        if (sleepPeriod > 0) {
            bslmt::ThreadUtil::microSleep(sleepPeriod, 0);
        }
    }

    rmqt::Result<> result = producer->waitForConfirms();
    if (!result) {
        BALL_LOG_ERROR << result;
    }
    else {
        BALL_LOG_INFO
            << threadName << ": Sent " << send_n
            << " messages, waiting for remaining broker confirmations";
    }
}

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);
    int send_n                 = 0;
    int sleepPeriod            = 5;
    int maxOutstandingConfirms = 5;
    int messageSize            = 100;
    int numberOfProducers      = 5;
    int numberOfConnections    = 2;

    balcl::OptionInfo extraArgs[] = {
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
            "How many microseconds to sleep after sending each message",
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
            "i|numberOfProducers",
            "numberOfProducers",
            "Number of producers per connection",
            balcl::TypeInfo(&numberOfProducers),
            balcl::OccurrenceInfo(numberOfProducers),
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

    rmqt::ErrorCallback errorCb = &onError;
    rmqa::RabbitContextOptions rabbitOptions;
    rabbitOptions.setErrorCallback(errorCb);
    rmqa::RabbitContext rabbit(rabbitOptions);
    bsl::vector<bsl::shared_ptr<rmqa::VHost> > connections;
    for (int j = 0; j < numberOfConnections; ++j) {
        connections.push_back(params.createConnection(rabbit, &j));
    }

    bslmt::ThreadGroup threadGroup;
    for (int j = 0; j < numberOfConnections; ++j) {
        for (int i = 0; i < numberOfProducers; ++i) {
            bsl::string threadName =
                "con" + bsl::to_string(j) + "prod" + bsl::to_string(i);
            threadGroup.addThread(bdlf::BindUtil::bind(&runPublishingTest,
                                                       connections[j],
                                                       params.queueName,
                                                       threadName,
                                                       maxOutstandingConfirms,
                                                       messageSize,
                                                       send_n,
                                                       sleepPeriod));
        }
    }

    threadGroup.joinAll();
    bsl::cout << sentCount << " Messages Sent" << bsl::endl;

    return 0;
}
