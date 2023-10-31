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

#include <rmqamqp_topologytransformer.h>

#include <rmqamqpt_exchangebind.h>
#include <rmqamqpt_exchangebindok.h>
#include <rmqamqpt_exchangedeclare.h>
#include <rmqamqpt_exchangedeclareok.h>
#include <rmqamqpt_exchangemethod.h>
#include <rmqamqpt_queuebind.h>
#include <rmqamqpt_queuebindok.h>
#include <rmqamqpt_queuedeclare.h>
#include <rmqamqpt_queuedeclareok.h>
#include <rmqamqpt_queuedelete.h>
#include <rmqamqpt_queuedeleteok.h>
#include <rmqamqpt_queuemethod.h>
#include <rmqt_topology.h>

#include <ball_log.h>

#include <bdlf_bind.h>
#include <bsl_algorithm.h>
#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_stdexcept.h>
#include <bsls_assert.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.TOPOLOGYTRANSFORMER")
} // namespace

TopologyTransformer::TopologyTransformer(const rmqt::Topology& topology)
: d_messages()
, d_sentMessageCount(0)
, d_receivedReplyCount(0)
, d_error(false)
, d_expectedReplies()
, d_expectedQueueNames()
{
    bsl::for_each(topology.queues.begin(),
                  topology.queues.end(),
                  bdlf::BindUtil::bind(&TopologyTransformer::processQueue,
                                       this,
                                       bdlf::PlaceHolders::_1));
    bsl::for_each(topology.exchanges.begin(),
                  topology.exchanges.end(),
                  bdlf::BindUtil::bind(&TopologyTransformer::processExchange,
                                       this,
                                       bdlf::PlaceHolders::_1));
    bsl::for_each(
        topology.queueBindings.begin(),
        topology.queueBindings.end(),
        bdlf::BindUtil::bind(&TopologyTransformer::processQueueBinding,
                             this,
                             bdlf::PlaceHolders::_1));

    bsl::for_each(
        topology.exchangeBindings.begin(),
        topology.exchangeBindings.end(),
        bdlf::BindUtil::bind(&TopologyTransformer::processExchangeBinding,
                             this,
                             bdlf::PlaceHolders::_1));
}

TopologyTransformer::TopologyTransformer(const rmqt::TopologyUpdate& topology)
: d_messages()
, d_sentMessageCount(0)
, d_receivedReplyCount(0)
, d_error(false)
, d_expectedReplies()
, d_expectedQueueNames()
{
    bsl::for_each(topology.updates.begin(),
                  topology.updates.end(),
                  bdlf::BindUtil::bind(&TopologyTransformer::processUpdate,
                                       this,
                                       bdlf::PlaceHolders::_1));
}

bool TopologyTransformer::hasNext() const
{
    return d_sentMessageCount < d_messages.size();
}

bool TopologyTransformer::hasError() const { return d_error; }

Message TopologyTransformer::getNextMessage()
{
    BSLS_ASSERT(d_sentMessageCount < d_messages.size());

    const Message& message = d_messages[d_sentMessageCount];
    ++d_sentMessageCount;
    return message;
}

bool TopologyTransformer::processReplyMessage(const Message& message)
{
    if (!message.is<rmqamqpt::Method>()) {
        d_error = true;
        return false;
    }

    if (d_expectedReplies.size() == 0) {
        BALL_LOG_ERROR << "TopologyTransformer::matchesExpectedReply called "
                          "when not expecting a reply";
        d_error = true;
        return false;
    }
    const Reply expectedReply = d_expectedReplies.front();
    d_expectedReplies.pop();

    const rmqamqpt::Method& method = message.the<rmqamqpt::Method>();
    bool isValidReply              = false;
    if (method.is<rmqamqpt::QueueMethod>()) {
        const rmqamqpt::QueueMethod& queueMethod =
            method.the<rmqamqpt::QueueMethod>();
        if (queueMethod.is<rmqamqpt::QueueDeclareOk>()) {
            const rmqamqpt::QueueDeclareOk& queueDeclareOk =
                queueMethod.the<rmqamqpt::QueueDeclareOk>();

            if (d_expectedQueueNames.size() == 0) {
                BALL_LOG_ERROR << "TopologyTransformer received unexpected "
                                  "queue.declare-ok reply";
                d_error = true;
                return false;
            }
            if (expectedReply == QUEUE) {
                if (d_expectedQueueNames.front().empty()) {
                    BALL_LOG_INFO << "Server-named queue declared: "
                                  << queueDeclareOk.name();
                    isValidReply = true;
                }
                else {
                    isValidReply =
                        d_expectedQueueNames.front() == queueDeclareOk.name();
                }
            }

            d_expectedQueueNames.pop();
        }
        else if (queueMethod.is<rmqamqpt::QueueBindOk>()) {
            isValidReply = expectedReply == QUEUE_BINDING;
        }
        else if (queueMethod.is<rmqamqpt::QueueUnbindOk>()) {
            isValidReply = expectedReply == QUEUE_UNBINDING;
        }
        else if (queueMethod.is<rmqamqpt::QueueDeleteOk>()) {
            BALL_LOG_INFO
                << "Deleted queue had "
                << queueMethod.the<rmqamqpt::QueueDeleteOk>().messageCount()
                << " messages";
            isValidReply = expectedReply == QUEUE_DELETE;
        }
    }
    else if (method.is<rmqamqpt::ExchangeMethod>()) {
        const rmqamqpt::ExchangeMethod& exchangeMethod =
            method.the<rmqamqpt::ExchangeMethod>();
        if (exchangeMethod.is<rmqamqpt::ExchangeDeclareOk>()) {
            isValidReply = expectedReply == EXCHANGE;
        }
        else if (exchangeMethod.is<rmqamqpt::ExchangeBindOk>()) {
            isValidReply = expectedReply == EXCHANGE_BINDING;
        }
    }
    else {
        BALL_LOG_ERROR << "Received unhandled topology method: " << method;
    }
    d_error = !isValidReply;
    return isValidReply;
}

bool TopologyTransformer::isDone() const
{
    return d_expectedReplies.empty() && d_sentMessageCount == d_messages.size();
}

void TopologyTransformer::processQueue(
    const bsl::shared_ptr<rmqt::Queue>& queue)
{
    rmqamqpt::QueueDeclare declare(queue->name(),
                                   queue->passive(),
                                   queue->durable(),
                                   false,
                                   queue->autoDelete(),
                                   false,
                                   queue->arguments());
    d_expectedReplies.push(QUEUE);
    d_expectedQueueNames.push(queue->name());
    d_messages.push_back(
        Message(rmqamqpt::Method(rmqamqpt::QueueMethod(declare))));
}

void TopologyTransformer::processExchange(
    const bsl::shared_ptr<rmqt::Exchange>& exchange)
{
    if (exchange->isDefault())
        return;
    rmqamqpt::ExchangeDeclare declare(exchange->name(),
                                      exchange->type(),
                                      exchange->passive(),
                                      exchange->durable(),
                                      exchange->autoDelete(),
                                      exchange->internal(),
                                      false,
                                      exchange->arguments());
    d_expectedReplies.push(EXCHANGE);
    d_messages.push_back(
        Message(rmqamqpt::Method(rmqamqpt::ExchangeMethod(declare))));
}

void TopologyTransformer::processQueueBinding(
    const bsl::shared_ptr<rmqt::QueueBinding>& binding)
{
    bsl::shared_ptr<rmqt::Queue> queue = binding->queue().lock();
    if (!queue) {
        BALL_LOG_ERROR
            << "Queue passed to bind to an exchange, was destructed.";
        d_error = true;
        return;
    }

    bsl::shared_ptr<rmqt::Exchange> exchange = binding->exchange().lock();
    if (!exchange) {
        BALL_LOG_ERROR << "Exchange passed to bind to a queue, was destructed.";
        d_error = true;
        return;
    }

    rmqamqpt::QueueBind bind(queue->name(),
                             exchange->name(),
                             binding->bindingKey(),
                             false,
                             binding->args());
    d_messages.push_back(
        Message(rmqamqpt::Method(rmqamqpt::QueueMethod(bind))));
    d_expectedReplies.push(QUEUE_BINDING);
}

void TopologyTransformer::processQueueUnbinding(
    const bsl::shared_ptr<rmqt::QueueUnbinding>& unbinding)
{
    bsl::shared_ptr<rmqt::Queue> queue = unbinding->queue().lock();
    if (!queue) {
        BALL_LOG_ERROR
            << "Queue passed to unbind from an exchange, was destructed.";
        d_error = true;
        return;
    }

    bsl::shared_ptr<rmqt::Exchange> exchange = unbinding->exchange().lock();
    if (!exchange) {
        BALL_LOG_ERROR << "Exchange passed to bind to a queue, was destructed.";
        d_error = true;
        return;
    }

    rmqamqpt::QueueUnbind bind(queue->name(),
                               exchange->name(),
                               unbinding->bindingKey(),
                               unbinding->args());
    d_messages.push_back(
        Message(rmqamqpt::Method(rmqamqpt::QueueMethod(bind))));
    d_expectedReplies.push(QUEUE_UNBINDING);
}

void TopologyTransformer::processQueueDelete(
    const bsl::shared_ptr<rmqt::QueueDelete>& queueDelete)
{
    rmqamqpt::QueueDelete qd(queueDelete->name(),
                             queueDelete->ifUnused(),
                             queueDelete->ifEmpty(),
                             queueDelete->noWait());
    d_messages.push_back(Message(rmqamqpt::Method(rmqamqpt::QueueMethod(qd))));
    d_expectedReplies.push(QUEUE_DELETE);
}

void TopologyTransformer::processExchangeBinding(
    const bsl::shared_ptr<rmqt::ExchangeBinding>& binding)
{
    bsl::shared_ptr<rmqt::Exchange> sourceX = binding->sourceExchange().lock();
    if (!sourceX) {
        BALL_LOG_ERROR << "Source exchange passed to bind to a destination "
                          "exchange, was destructed.";
        d_error = true;
        return;
    }

    bsl::shared_ptr<rmqt::Exchange> destinationX =
        binding->destinationExchange().lock();
    if (!destinationX) {
        BALL_LOG_ERROR << "Destination exchange passed to bind to a source "
                          "exchange, was destructed.";
        d_error = true;
        return;
    }

    rmqamqpt::ExchangeBind bind(sourceX->name(),
                                destinationX->name(),
                                binding->bindingKey(),
                                false,
                                binding->args());
    d_messages.push_back(
        Message(rmqamqpt::Method(rmqamqpt::ExchangeMethod(bind))));
    d_expectedReplies.push(EXCHANGE_BINDING);
}

void TopologyTransformer::processUpdate(
    const rmqt::TopologyUpdate::SupportedUpdate& update)
{
    if (update.is<bsl::shared_ptr<rmqt::QueueBinding> >()) {
        processQueueBinding(update.the<bsl::shared_ptr<rmqt::QueueBinding> >());
    }
    else if (update.is<bsl::shared_ptr<rmqt::QueueUnbinding> >()) {
        processQueueUnbinding(
            update.the<bsl::shared_ptr<rmqt::QueueUnbinding> >());
    }
    else if (update.is<bsl::shared_ptr<rmqt::QueueDelete> >()) {
        processQueueDelete(update.the<bsl::shared_ptr<rmqt::QueueDelete> >());
    }
    else {
        BALL_LOG_ERROR << "Update type not supported";
        d_error = true;
    }
}

} // namespace rmqamqp
} // namespace BloombergLP
