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
#include <rmqt_plaincredentials.h>

#include <ball_log.h>
#include <bslmt_threadutil.h>
#include <bsls_atomic.h>

#include <bsl_memory.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.CANCELCONSUMER")
}

// Consumer callback
class MessageConsumer {
  private:
    bsls::AtomicBool& d_done;

  public:
    explicit MessageConsumer(bsls::AtomicBool& done)
    : d_done(done)
    {
    }

    bool done() { return d_done.load(); }

    void operator()(rmqp::MessageGuard& guard)
    {
        bsl::string msg((const char*)guard.message().payload(),
                        guard.message().payloadSize());
        BALL_LOG_INFO << "Received message: " << guard.message()
                      << " Content: " << msg;
        if (msg == "bad") {
            guard.consumer()->cancel();
            d_done = true;
        }
        d_done ? guard.nack() : guard.ack();
    }
};

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    int prefetch = 100;

    balcl::OptionInfo extraArgs[] = {
        {
            "l|prefetch",
            "prefetch",
            "Prefetch count",
            balcl::TypeInfo(&prefetch),
            balcl::OccurrenceInfo(prefetch),
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

    bsls::AtomicBool done;
    rmqt::Result<rmqa::Consumer> consumer = connection->createConsumer(
        topology, queue, MessageConsumer(done), params.consumerTag, prefetch);

    while (!done) {
        bslmt::ThreadUtil::microSleep(0, 5); // sleep 5s
    }
}
