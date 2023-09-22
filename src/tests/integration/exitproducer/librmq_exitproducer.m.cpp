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
#include <rmqa_vhost.h>
#include <rmqt_future.h>

#include <balcl_commandline.h>
#include <ball_log.h>
#include <bslmt_threadutil.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.EXITPRODUCER")
}

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
    int sleepPeriod            = 5;
    int maxOutstandingConfirms = 5;
    int numProducers           = 1;
    int numMessages            = 0;
    bool waitForConfirms       = false;

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
            "How many seconds to sleep after destroying the producers and "
            "before exiting the process",
            balcl::TypeInfo(&sleepPeriod),
            balcl::OccurrenceInfo(sleepPeriod),
        },
        {
            "l|maxOutstandingConfirms",
            "maxOutstandingConfirms",
            "The max number of outstanding confirms",
            balcl::TypeInfo(&maxOutstandingConfirms),
            balcl::OccurrenceInfo(maxOutstandingConfirms),
        },
        {
            "numProducers",
            "numProducers",
            "The number of producers to declare",
            balcl::TypeInfo(&numProducers),
            balcl::OccurrenceInfo(numProducers),
        },
        {
            "numMessages",
            "numMessages",
            "The number of messages to publish to all producers with a 60 "
            "second delay in between each batch",
            balcl::TypeInfo(&numMessages),
            balcl::OccurrenceInfo(numMessages),
        },
        {
            "waitForConfirms",
            "waitForConfirms",
            "Delay exit until all produced messages are confirmed",
            balcl::TypeInfo(&waitForConfirms),
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
    rmqa::RabbitContext rabbit(rabbitOptions);

    rmqa::Topology topology;
    rmqt::ExchangeHandle exch1 = topology.addExchange(
        "source-exchange", rmqt::ExchangeType::FANOUT, rmqt::AutoDelete::ON);
    rmqt::ExchangeHandle exch2 =
        topology.addExchange("destination-exchange",
                             rmqt::ExchangeType::DIRECT,
                             rmqt::AutoDelete::ON);
    topology.bind(exch1, exch2, "");
    rmqt::QueueHandle queue = topology.addQueue(params.queueName);
    topology.bind(exch2, queue, params.queueName);

    bsl::shared_ptr<rmqa::VHost> vhost = params.createConnection(rabbit);
    bsl::vector<rmqt::Future<rmqa::Producer> > producers;

    for (int i = 0; i < numProducers; i++) {
        producers.push_back(vhost->createProducerAsync(
            topology, exch1, maxOutstandingConfirms));
    }

    if (numMessages > 0) {
        const bsl::string messageText(100, '*');

        for (int i = 0; i < numMessages; i++) {
            bslmt::ThreadUtil::microSleep(0, 60);
            rmqt::Message message(bsl::make_shared<bsl::vector<uint8_t> >(
                messageText.cbegin(), messageText.cend()));

            for (bsl::vector<rmqt::Future<rmqa::Producer> >::iterator it =
                     producers.begin();
                 it != producers.end();
                 ++it) {
                it->blockResult().value()->send(
                    message, "hello", &onConfirmUpdate);
            }
            BALL_LOG_INFO << "Published a batch of messages";
        }
    }

    if (waitForConfirms) {
        for (bsl::vector<rmqt::Future<rmqa::Producer> >::iterator it =
                 producers.begin();
             it != producers.end();
             ++it) {
            it->blockResult().value()->waitForConfirms();
        }
    }

    producers.clear();

    bslmt::ThreadUtil::microSleep(0, sleepPeriod);

    return 0;
}
