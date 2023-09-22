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

#ifndef INCLUDED_RMQPERFTEST_ARGS
#define INCLUDED_RMQPERFTEST_ARGS

#include <bsl_string.h>

namespace BloombergLP {
namespace rmqperftest {

struct TopologyArgs {
    TopologyArgs()
    : autoDelete("true")
    , exchangeName("")
    , queueName("perftestqueue")
    , queuePattern("")
    , queuePatternStartNum(0)
    , queuePatternEndNum(0)
    , exchangeType("direct")
    {
    }

    bsl::string autoDelete;
    bsl::string exchangeName;
    bsl::string queueName;
    bsl::string queuePattern;
    int queuePatternStartNum;
    int queuePatternEndNum;
    bsl::string exchangeType;

    // TODO bsl::string queueArgs;
    //  expects comma separated "key=value, key=value, ..."
};

struct ProducerArgs {
    ProducerArgs()
    : maxOutstandingConfirms(100)
    , producerMessageCount(-1)
    , routingKey()
    , publishingInterval(0.0)
    , messageSize(10)
    , messageFlag("")
    , numProducers(1)
    {
    }

    int maxOutstandingConfirms;
    int producerMessageCount;
    bsl::string routingKey;
    double publishingInterval;
    int messageSize;
    bsl::string messageFlag;
    int numProducers;

    // TODO bsl::string messageContentType;
    // TODO double producerRandomStartDelay = 0.0;
    // TODO int producerRateLimit = 0;
};

struct ConsumerArgs {
    ConsumerArgs()
    : consumerMessageCount(-1)
    , prefetch(100)
    , consumerRateLimit(0)
    , numConsumers(1)
    , consumerArgs("")
    {
    }

    int consumerMessageCount;
    int prefetch;
    int consumerRateLimit;
    int numConsumers;
    bsl::string
        consumerArgs; // expected comma separated "key=value, key=value, ..."

    // TODO bool consumerSlowStart = false;
    // TODO int consumerLatencyUSec  = 0;
    // TODO bool nackRequeueMsg      = false;
};

struct PerfTestArgs {
    PerfTestArgs()
    : testId()
    , uri("amqp://localhost/")
    , testRunDuration(0)
    , topology()
    , consumer()
    , producer()
    , shuffleConnectionEndpoints(false)
    {
    }

    bsl::string testId;
    bsl::string uri;
    int testRunDuration;

    TopologyArgs topology;

    ConsumerArgs consumer;
    ProducerArgs producer;

    bool shuffleConnectionEndpoints;
};

} // namespace rmqperftest
} // namespace BloombergLP

#endif
