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

#ifndef INCLUDED_RMQAMQP_CONSUMER_H
#define INCLUDED_RMQAMQP_CONSUMER_H

#include <rmqamqp_channel.h>
#include <rmqamqp_message.h>
#include <rmqamqp_messagestore.h>
#include <rmqamqp_multipleackhandler.h>

#include <rmqio_serializedframe.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_consumerconfig.h>
#include <rmqt_envelope.h>
#include <rmqt_future.h>
#include <rmqt_queue.h>
#include <rmqt_result.h>
#include <rmqt_topology.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_unordered_map.h>
#include <bsls_keyword.h>

namespace BloombergLP {
namespace rmqamqp {

//=======
// Class Consumer
//=======

class Consumer {
  public:
    typedef bsl::function<void(const rmqt::Message&, const rmqt::Envelope&)>
        MessageCallback;

    enum State {
        NOT_CONSUMING,
        STARTING,
        CONSUMING,
        CANCELLING_BUT_RESUMING,
        CANCELLING,
        CANCELLED
    };
    bsl::optional<rmqamqpt::BasicMethod>
    consume(const rmqt::ConsumerConfig& consumerConfig);
    bsl::optional<rmqamqpt::BasicMethod> consumeOk();
    void process(const rmqt::Message& message,
                 const rmqamqpt::BasicDeliver& deliver,
                 size_t lifetimeId);

    /// Signal the server to stop delivering
    bsl::optional<rmqamqpt::BasicMethod> cancel();

    /// Server acknowledged cancel
    void cancelOk();

    ~Consumer();

    Consumer(const bsl::shared_ptr<rmqt::Queue>& queue,
             const MessageCallback& onNewMessage,
             const bsl::string& tag);

    bool isStarted() const;
    bool isActive() const;
    bool isCancelling() const;

    void reset();
    void resume();

    const bsl::string& consumerTag() const;
    const bsl::string& queueName() const;

  private:
    State d_state;
    bsl::string d_tag;
    bsl::shared_ptr<rmqt::Queue> d_queue;
    MessageCallback d_onNewMessage;
};
} // namespace rmqamqp
} // namespace BloombergLP
#endif