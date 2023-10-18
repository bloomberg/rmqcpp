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

#ifndef INCLUDED_RMQAMQP_TOPOLOGYTRANSFORMER
#define INCLUDED_RMQAMQP_TOPOLOGYTRANSFORMER

#include <rmqamqp_message.h>

#include <rmqt_topology.h>
#include <rmqt_topologyupdate.h>

#include <bsl_memory.h>
#include <bsl_queue.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {

//@PURPOSE: Transform rmqt::Topology into a corresponding sequence of
// AMQP messages to declare the topology to the broker
//
//@CLASSES:
//  rmqamqp::TopologyTransformer: takes an rmqt::Topology and
//  provides an iterator-like interface for transforming it into AMQP messages

class TopologyTransformer {
  public:
    explicit TopologyTransformer(const rmqt::Topology& topology);
    explicit TopologyTransformer(const rmqt::TopologyUpdate& topology);

    /// Returns true iff there is at least one more message to be sent.
    bool hasNext() const;

    /// Returns next message to be sent to the broker.
    Message getNextMessage();

    /// Takes a declaration/binding reply message as argument. Returns true iff
    /// the reply is a valid declare-ok / bind-ok message.
    bool processReplyMessage(const Message& message);

    /// Returns true iff the entire topology has been declared. This is
    /// confirmed by counting the reply messages.
    bool isDone() const;

    /// Returns true if there is any error, while declaring topology
    bool hasError() const;

  private:
    void processQueue(const bsl::shared_ptr<rmqt::Queue>& queue);
    void processExchange(const bsl::shared_ptr<rmqt::Exchange>& exchange);
    void processQueueBinding(const bsl::shared_ptr<rmqt::QueueBinding>&);
    void processQueueUnbinding(
        const bsl::shared_ptr<rmqt::QueueUnbinding>& unbinding);
    void
    processQueueDelete(const bsl::shared_ptr<rmqt::QueueDelete>& queueDelete);
    void processExchangeBinding(const bsl::shared_ptr<rmqt::ExchangeBinding>&);
    void processUpdate(const rmqt::TopologyUpdate::SupportedUpdate&);

  private:
    bsl::vector<Message> d_messages;

    size_t d_sentMessageCount;
    size_t d_receivedReplyCount;
    bool d_error;

    // Expected replies
    enum Reply {
        QUEUE,
        EXCHANGE,
        QUEUE_BINDING,
        EXCHANGE_BINDING,
        QUEUE_UNBINDING,
        QUEUE_DELETE
    };
    bsl::queue<Reply> d_expectedReplies;
    bsl::queue<bsl::string> d_expectedQueueNames;
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
