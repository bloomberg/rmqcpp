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

#include <rmqa_producerimpl.h>

#include <rmqp_producertracing.h>

#include <rmqa_tracingproducerimpl.h>

#include <rmqtestutil_mockchannel.t.h>
#include <rmqtestutil_mockeventloop.t.h>
#include <rmqtestutil_savethreadid.h>

#include <rmqp_producer.h>
#include <rmqt_confirmresponse.h>
#include <rmqt_message.h>
#include <rmqt_queue.h>
#include <rmqt_simpleendpoint.h>
#include <rmqt_topology.h>

#include <bdlf_bind.h>
#include <bslmt_threadutil.h>
#include <bsls_systemtime.h>
#include <bsls_timeinterval.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace ::testing;
using namespace bdlf::PlaceHolders;

namespace {

class ConfirmCallback {
  public:
    virtual ~ConfirmCallback() {}
    virtual void onConfirm(const rmqt::Message&,
                           const bsl::string& routingKey,
                           const rmqt::ConfirmResponse& confirmResponse) = 0;
};

class MockProducerTracing : public rmqp::ProducerTracing {
  public:
    struct MockContext : rmqp::ProducerTracing::Context {
        MOCK_METHOD1(response, void(const rmqt::ConfirmResponse& confirm));
    };

    MockProducerTracing()
    {
        ON_CALL(*this, createAndTag(_, _, _, _))
            .WillByDefault(Return(bsl::make_shared<MockContext>()));
    }

    MOCK_CONST_METHOD4(
        createAndTag,
        bsl::shared_ptr<Context>(
            rmqt::Properties* properties,
            const bsl::string& routingKey,
            const bsl::string& exchangeName,
            const bsl::shared_ptr<const rmqt::Endpoint>& endpoint));
};

class MockConfirmCallback : public ConfirmCallback {
  public:
    MOCK_METHOD3(onConfirm,
                 void(const rmqt::Message&,
                      const bsl::string&,
                      const rmqt::ConfirmResponse& confirmResponse));
};

MATCHER_P(ExchangeHandleNameEq, expected, "")
{
    bsl::shared_ptr<rmqt::Exchange> exch = arg.lock();

    if (!exch) {
        return false;
    }

    return exch->name() == expected->name();
}

enum ProducerType { PRODUCER, TRACING_PRODUCER };

} // namespace

class ProducerImplTests : public TestWithParam<ProducerType> {
  public:
    struct PrintParamName {
        template <class ParamType>
        const char* operator()(const TestParamInfo<ParamType>& info) const
        {
            switch (info.param) {
                case TRACING_PRODUCER:
                    return "TracingProducer";
                default:
                    return "Producer";
            }
        }
    };

  protected:
    bsl::shared_ptr<rmqt::Queue> d_queue;
    bsl::shared_ptr<MockConfirmCallback> d_mockCallback;
    rmqp::Producer::ConfirmationCallback d_callback; // Calls d_mockCallback
    bdlmt::ThreadPool d_threadPool;
    rmqtestutil::MockEventLoop d_eventLoop;
    bsl::shared_ptr<StrictMock<rmqtestutil::MockSendChannel> >
        d_mockSendChannel;
    rmqt::Topology d_topology;
    rmqt::Message d_message;
    bsl::shared_ptr<rmqt::Exchange> d_exchange;
    bsls::TimeInterval d_timeout;
    bsl::shared_ptr<MockProducerTracing> d_tracing;
    bsl::shared_ptr<rmqa::ProducerImpl::Factory> d_factory;

    ProducerImplTests()
    : d_queue(bsl::make_shared<rmqt::Queue>("test-queue"))
    , d_mockCallback(bsl::make_shared<MockConfirmCallback>())
    , d_callback(bdlf::BindUtil::bind(&MockConfirmCallback::onConfirm,
                                      d_mockCallback.get(),
                                      _1,
                                      _2,
                                      _3))
    , d_threadPool(bslmt::ThreadAttributes(), 0, 5, 5)
    , d_eventLoop()
    , d_mockSendChannel(
          bsl::make_shared<StrictMock<rmqtestutil::MockSendChannel> >())
    , d_topology()
    , d_message(bsl::make_shared<bsl::vector<uint8_t> >(5))
    , d_exchange(bsl::make_shared<rmqt::Exchange>("test-exchange"))
    , d_tracing(bsl::make_shared<MockProducerTracing>())
    , d_factory(paramPicker(GetParam()))
    {
        d_topology.queues.push_back(d_queue);
        d_topology.exchanges.push_back(d_exchange);
        d_threadPool.start();

        EXPECT_CALL(d_eventLoop, postImpl(_))
            .WillRepeatedly(InvokeArgument<0>());
    }

    rmqt::Message newMessage()
    {
        return rmqt::Message(bsl::make_shared<bsl::vector<uint8_t> >());
    }

    bsl::shared_ptr<rmqa::ProducerImpl::Factory> paramPicker(ProducerType pt)
    {
        switch (pt) {
            case TRACING_PRODUCER:
                return bsl::make_shared<rmqa::TracingProducerImpl::Factory>(
                    bsl::make_shared<rmqt::SimpleEndpoint>("example-hostname",
                                                           "example-vhost"),
                    d_tracing);
            default:
                return bsl::make_shared<rmqa::ProducerImpl::Factory>();
        }
    }
};

TEST_P(ProducerImplTests, ItsAlive)
{
    EXPECT_CALL(*d_mockSendChannel, setCallback(_));
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));
}

TEST_P(ProducerImplTests, PublishToQueueCallsAmqpChannel)
{
    // Check producer::send(queue) invokes SendChannel::publishMessage correctly

    EXPECT_CALL(*d_mockSendChannel, setCallback(_));
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    // Ensure we publish to an with the queue name as routing key
    EXPECT_CALL(*d_mockSendChannel, publishMessage(_, d_queue->name(), _));
    EXPECT_THAT(
        producer->send(d_message, d_queue->name(), d_callback, d_timeout),
        Eq(rmqp::Producer::SENDING));

    d_threadPool.drain();
}

TEST_P(ProducerImplTests, PublishToExchangeCallsAmqpChannel)
{
    // Check producer::send(queue) invokes SendChannel::publishMessage correctly

    EXPECT_CALL(*d_mockSendChannel, setCallback(_));
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    // Ensure we publish with the correct exchange & routingKey & default
    // mandatory flag
    EXPECT_CALL(*d_mockSendChannel,
                publishMessage(_,
                               bsl::string("routingKey"),
                               rmqt::Mandatory::RETURN_UNROUTABLE));

    producer->send(d_message, "routingKey", d_callback, d_timeout);

    d_threadPool.drain();
}

TEST_P(ProducerImplTests, PublishNotMandatory)
{
    // Check producer::send sends the correct mandatory flag

    EXPECT_CALL(*d_mockSendChannel, setCallback(_));
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    // Ensure we publish with the correct exchange & routingKey & default
    // mandatory flag
    EXPECT_CALL(*d_mockSendChannel,
                publishMessage(_,
                               bsl::string("routingKey"),
                               rmqt::Mandatory::DISCARD_UNROUTABLE));

    producer->send(d_message,
                   "routingKey",
                   rmqt::Mandatory::DISCARD_UNROUTABLE,
                   d_callback,
                   d_timeout);

    d_threadPool.drain();
}

TEST_P(ProducerImplTests, DuplicateMessagesReturnDuplicate)
{
    // Ensure sending two msgs to a producer with the same GUID will return
    // DUPLICATE on the second call

    EXPECT_CALL(*d_mockSendChannel, setCallback(_));
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        2, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    EXPECT_CALL(*d_mockSendChannel, publishMessage(_, _, _));
    EXPECT_THAT(
        producer->send(d_message, d_queue->name(), d_callback, d_timeout),
        Eq(rmqp::Producer::SENDING));

    // no confirm call in between - still outstanding

    EXPECT_THAT(
        producer->send(d_message, d_queue->name(), d_callback, d_timeout),
        Eq(rmqp::Producer::DUPLICATE));
}

class ProducerImplMaxOutstandingTests : public ProducerImplTests {
  public:
    ProducerImplMaxOutstandingTests()
    {
        EXPECT_CALL(*d_mockSendChannel, publishMessage(_, _, _))
            .WillRepeatedly(Return());

        EXPECT_CALL(*d_mockSendChannel, setCallback(_))
            .WillOnce(SaveArg<0>(&d_injectConfirm));
    }

    rmqamqp::SendChannel::MessageConfirmCallback d_injectConfirm;
};

TEST_P(ProducerImplMaxOutstandingTests, SequentialMessagesDoNotBlock)
{
    // Ensure sending two msgs to a producer with max oustanding of 1 does not
    // block if we confirm the first
    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);

    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    {
        EXPECT_THAT(
            producer->send(d_message, d_queue->name(), d_callback, d_timeout),
            Eq(rmqp::Producer::SENDING));

        EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse))
            .WillOnce(Return());
        d_injectConfirm(d_message, d_queue->name(), confirmResponse);

        d_threadPool.drain();
    } // Push the expectation out of scope so we can expect it again

    d_threadPool.start();

    {
        EXPECT_THAT(
            producer->send(d_message, d_queue->name(), d_callback, d_timeout),
            Eq(rmqp::Producer::SENDING));

        EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse));
        d_injectConfirm(d_message, d_queue->name(), confirmResponse);

        d_threadPool.drain();
    }
}

TEST_P(ProducerImplMaxOutstandingTests, SequentialTrySendMessagesDoNotBlock)
{
    // Ensure sending two msgs to a producer with max oustanding of 1 does not
    // block if we confirm the first
    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);

    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    {
        EXPECT_THAT(producer->trySend(d_message, d_queue->name(), d_callback),
                    Eq(rmqp::Producer::SENDING));

        EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse))
            .WillOnce(Return());
        d_injectConfirm(d_message, d_queue->name(), confirmResponse);

        d_threadPool.drain();
    } // Push the expectation out of scope so we can expect it again

    d_threadPool.start();

    {
        EXPECT_THAT(producer->trySend(d_message, d_queue->name(), d_callback),
                    Eq(rmqp::Producer::SENDING));

        EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse));
        d_injectConfirm(d_message, d_queue->name(), confirmResponse);

        d_threadPool.drain();
    }
}

TEST_P(ProducerImplMaxOutstandingTests, TrySendReachesLimit)
{
    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);

    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    {
        EXPECT_THAT(producer->trySend(d_message, d_queue->name(), d_callback),
                    Eq(rmqp::Producer::SENDING));

        EXPECT_THAT(producer->trySend(d_message, d_queue->name(), d_callback),
                    Eq(rmqp::Producer::INFLIGHT_LIMIT));

        EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse))
            .WillOnce(Return());
        d_injectConfirm(d_message, d_queue->name(), confirmResponse);

        d_threadPool.drain();
    }

    d_threadPool.start();

    {
        EXPECT_THAT(producer->trySend(d_message, d_queue->name(), d_callback),
                    Eq(rmqp::Producer::SENDING));

        EXPECT_THAT(producer->trySend(d_message, d_queue->name(), d_callback),
                    Eq(rmqp::Producer::INFLIGHT_LIMIT));

        EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse));
        d_injectConfirm(d_message, d_queue->name(), confirmResponse);

        d_threadPool.drain();
    }
}

TEST_P(ProducerImplMaxOutstandingTests, InflightLimitTimesOutSecondSend)
{
    // Ensure sending two msgs to a producer with max oustanding of 1 does not
    // block if we confirm the first
    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);

    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    d_timeout = bsls::TimeInterval(0, 50000000); // 50 milliseconds

    rmqt::Message msg1 = newMessage(), msg2 = newMessage(), msg3 = newMessage();
    {
        EXPECT_THAT(
            producer->send(
                msg1, d_queue->name(), d_callback, bsls::TimeInterval()),
            Eq(rmqp::Producer::SENDING));

        EXPECT_THAT(
            producer->send(msg2, d_queue->name(), d_callback, d_timeout),
            Eq(rmqp::Producer::TIMEOUT));

        EXPECT_CALL(*d_mockCallback, onConfirm(msg1, _, confirmResponse))
            .WillOnce(Return());
        d_injectConfirm(msg1, d_queue->name(), confirmResponse);
        d_threadPool.drain();
    } // Push the expectation out of scope so we can expect it again

    d_threadPool.start();

    {
        EXPECT_THAT(
            producer->send(
                msg2, d_queue->name(), d_callback, bsls::TimeInterval()),
            Eq(rmqp::Producer::SENDING));

        EXPECT_THAT(
            producer->send(msg3, d_queue->name(), d_callback, d_timeout),
            Eq(rmqp::Producer::TIMEOUT));

        EXPECT_CALL(*d_mockCallback, onConfirm(msg2, _, confirmResponse));
        d_injectConfirm(msg2, d_queue->name(), confirmResponse);

        EXPECT_THAT(
            producer->send(
                msg3, d_queue->name(), d_callback, bsls::TimeInterval()),
            Eq(rmqp::Producer::SENDING));

        d_threadPool.drain();
    }

    d_threadPool.start();

    {
        EXPECT_CALL(*d_mockCallback, onConfirm(msg3, _, confirmResponse));
        d_injectConfirm(msg3, d_queue->name(), confirmResponse);
        d_threadPool.drain();
    }
}

class ProducerImplConfirmTypeTests : public ProducerImplTests {
  public:
    bsl::shared_ptr<rmqa::ProducerImpl>
        d_producer; // shared_ptr to defer construction
    rmqamqp::SendChannel::MessageConfirmCallback d_injectConfirm;

    ProducerImplConfirmTypeTests()
    : d_producer()
    {
    }

    virtual void SetUp()
    {
        // Ensure we publish to an with the queue name as routing key
        EXPECT_CALL(*d_mockSendChannel, setCallback(_))
            .WillOnce(SaveArg<0>(&d_injectConfirm));
        d_producer =
            bsl::make_shared<rmqa::ProducerImpl>(1,
                                                 d_mockSendChannel,
                                                 bsl::ref(d_threadPool),
                                                 bsl::ref(d_eventLoop));

        EXPECT_CALL(*d_mockSendChannel, publishMessage(_, _, _));
        EXPECT_THAT(
            d_producer->send(d_message, d_queue->name(), d_callback, d_timeout),
            Eq(rmqp::Producer::SENDING));
    }

    bool waitForConfirms()
    {
        return d_producer->waitForConfirms(bsls::TimeInterval(0, 1)); // 1 ns
    }
};

TEST_P(ProducerImplConfirmTypeTests, SuccessfulConfirmCallsAccepted)
{
    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);

    EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse));

    d_injectConfirm(d_message, d_queue->name(), confirmResponse);

    d_threadPool.drain();
}

TEST_P(ProducerImplConfirmTypeTests, RejectedConfirmCallsReturned)
{
    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::REJECT);

    EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse));

    d_injectConfirm(d_message, d_queue->name(), confirmResponse);

    d_threadPool.drain();
}

TEST_P(ProducerImplConfirmTypeTests, CheckCallbackIsOnDifferentThread)
{
    // rmqa must call user back on different thread to the rmqamqp callback
    // Check the threadids to confirm

    uint64_t threadid = 0;
    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);
    EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse))
        .WillOnce(Invoke(bdlf::BindUtil::bind(&rmqtestutil::saveThreadId,
                                              bsl::ref(threadid))));

    d_injectConfirm(d_message, d_queue->name(), confirmResponse);

    EXPECT_THAT(threadid, Ne(bslmt::ThreadUtil::selfIdAsInt()));

    d_threadPool.drain();
}

TEST_P(ProducerImplConfirmTypeTests, WaitForConfirmsSuccess)
{
    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);
    EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse));
    d_injectConfirm(d_message, d_queue->name(), confirmResponse);

    d_threadPool.drain();

    ASSERT_TRUE(waitForConfirms());
}

TEST_P(ProducerImplConfirmTypeTests, WaitForConfirmsFails)
{
    d_threadPool.drain();

    ASSERT_FALSE(waitForConfirms());
}

TEST_P(ProducerImplConfirmTypeTests, MultipleWaitForConfirms)
{
    // Ensure calls to waitForConfirm are not destructive

    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);

    ASSERT_FALSE(waitForConfirms());
    ASSERT_FALSE(waitForConfirms());

    EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse));
    d_injectConfirm(d_message, d_queue->name(), confirmResponse);

    d_threadPool.drain();

    ASSERT_TRUE(waitForConfirms());
    ASSERT_TRUE(waitForConfirms());
}

struct WaitForConfirmsJob {
    explicit WaitForConfirmsJob(rmqa::ProducerImpl& producer)
    : d_producer(producer)
    {
    }

    void operator()() { d_producer.waitForConfirms(); }

    rmqa::ProducerImpl& d_producer;
};

TEST_P(ProducerImplConfirmTypeTests, ConcurrentWaitForConfirms)
{
    // Ensure multiple calls to waitForConfirms do not break each other

    d_threadPool.enqueueJob(WaitForConfirmsJob(*d_producer));
    d_threadPool.enqueueJob(WaitForConfirmsJob(*d_producer));

    rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);
    EXPECT_CALL(*d_mockCallback, onConfirm(_, _, confirmResponse));
    d_injectConfirm(d_message, d_queue->name(), confirmResponse);

    d_threadPool.drain();
    d_threadPool.start();

    d_threadPool.enqueueJob(WaitForConfirmsJob(*d_producer));
    d_threadPool.enqueueJob(WaitForConfirmsJob(*d_producer));

    d_threadPool.drain();
}

class ConfirmCallbackCheckUseCount {
  public:
    ConfirmCallbackCheckUseCount(const bsl::shared_ptr<int>& counter)
    : d_counter(counter)
    {
    }

    void operator()(const rmqt::Message&,
                    const bsl::string&,
                    const rmqt::ConfirmResponse&)
    {
        *d_counter = d_counter.use_count();
    }

    bsl::shared_ptr<int> d_counter;
};

class ProducerImplCallbackLifetimeTests
: public ProducerImplMaxOutstandingTests {};

TEST_P(ProducerImplCallbackLifetimeTests, CallbackStaysAlive)
{
    // Ensure a callbacks are kept alive for the duration of the call

    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        2, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    bsl::shared_ptr<int> counter = bsl::make_shared<int>(0);

    EXPECT_CALL(*d_mockSendChannel, publishMessage(_, _, _));
    EXPECT_THAT(producer->send(d_message,
                               d_queue->name(),
                               ConfirmCallbackCheckUseCount(counter),
                               d_timeout),
                Eq(rmqp::Producer::SENDING));

    // On injectConfirm - ensure a shared_ptr bound to callback has a use_count
    //     of 2 or more:
    // 1. Held by this function
    // 2. Held by the producer internals.

    const rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);
    d_injectConfirm(d_message, d_queue->name(), confirmResponse);

    EXPECT_TRUE(producer->waitForConfirms());

    EXPECT_THAT(*counter, Eq(2));
}

TEST_P(ProducerImplCallbackLifetimeTests, CallbackCancelledOnTimeout)
{
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        2, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    bsl::shared_ptr<int> counter = bsl::make_shared<int>(0);

    EXPECT_CALL(*d_mockSendChannel, publishMessage(_, _, _));
    EXPECT_THAT(producer->send(d_message,
                               d_queue->name(),
                               ConfirmCallbackCheckUseCount(counter),
                               d_timeout),
                Eq(rmqp::Producer::SENDING));

    bsls::TimeInterval currTime =
        bsls::SystemTime::now(bsls::SystemClockType::e_REALTIME);
    bsls::TimeInterval timeout(0, 1000 * 1000 * 100); // 100 ms
    EXPECT_FALSE(producer->waitForConfirms(timeout));
    bsls::TimeInterval diff =
        bsls::SystemTime::now(bsls::SystemClockType::e_REALTIME) - currTime;
    // Relax the expectation of sleep length a bit
    EXPECT_GE(diff.totalMilliseconds(), timeout.totalMilliseconds() * 0.9);

    EXPECT_THAT(*counter, Eq(0));
}

class ProducerImplUpdateTopology : public ProducerImplTests {};

TEST_P(ProducerImplUpdateTopology, UpdateCallback)
{
    bsl::shared_ptr<rmqtestutil::MockUpdateSendChannel> mockUpdateSendChannel =
        bsl::make_shared<rmqtestutil::MockUpdateSendChannel>();
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        2, d_exchange, mockUpdateSendChannel, d_threadPool, d_eventLoop));

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    EXPECT_CALL(d_eventLoop, postImpl(_)).Times(3);
    rmqt::Future<> future = producer->updateTopologyAsync(topologyUpdate);

    rmqamqp::Message message;
    message = rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueBindOk()));
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateSendChannel,
                             bsl::ref(message)));
    EXPECT_TRUE(future.blockResult());
}

TEST_P(ProducerImplUpdateTopology, BindUnbind)
{
    bsl::shared_ptr<rmqtestutil::MockUpdateSendChannel> mockUpdateSendChannel =
        bsl::make_shared<rmqtestutil::MockUpdateSendChannel>();
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        2, d_exchange, mockUpdateSendChannel, d_threadPool, d_eventLoop));

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));

    bsl::shared_ptr<rmqt::QueueUnbinding> queueUnbindingPtr =
        bsl::make_shared<rmqt::QueueUnbinding>(
            exchangePtr, queuePtr, "bindKey");
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueUnbindingPtr));

    EXPECT_CALL(d_eventLoop, postImpl(_)).Times(4);
    rmqt::Future<> future = producer->updateTopologyAsync(topologyUpdate);

    rmqamqp::Message bindOk;
    bindOk = rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueBindOk()));
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateSendChannel,
                             bsl::ref(bindOk)));
    EXPECT_FALSE(future.tryResult());

    rmqamqp::Message unbindOk;
    unbindOk =
        rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueUnbindOk()));
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateSendChannel,
                             bsl::ref(unbindOk)));
    EXPECT_TRUE(future.blockResult());
}

TEST_P(ProducerImplUpdateTopology, UpdateCallbackFromTwoThreadsAtOnce)
{
    bsl::shared_ptr<rmqtestutil::MockUpdateSendChannel> mockUpdateSendChannel =
        bsl::make_shared<rmqtestutil::MockUpdateSendChannel>();
    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        2, d_exchange, mockUpdateSendChannel, d_threadPool, d_eventLoop));

    bsl::shared_ptr<rmqt::Exchange> exchangePtr =
        bsl::make_shared<rmqt::Exchange>("exchange");
    bsl::shared_ptr<rmqt::Queue> queuePtr =
        bsl::make_shared<rmqt::Queue>("queue");
    bsl::shared_ptr<rmqt::QueueBinding> queueBindingPtr =
        bsl::make_shared<rmqt::QueueBinding>(exchangePtr, queuePtr, "bindKey");
    rmqt::TopologyUpdate topologyUpdate;
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBindingPtr));
    EXPECT_CALL(d_eventLoop, postImpl(_)).Times(2);
    rmqt::Future<> future1 = producer->updateTopologyAsync(topologyUpdate);
    rmqt::Future<> future2 = producer->updateTopologyAsync(topologyUpdate);
    Mock::VerifyAndClearExpectations(&d_eventLoop);

    EXPECT_CALL(d_eventLoop, postImpl(_)).Times(3);
    rmqamqp::Message bindOk;
    bindOk = rmqamqpt::Method(rmqamqpt::QueueMethod(rmqamqpt::QueueBindOk()));
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateSendChannel,
                             bsl::ref(bindOk)));
    EXPECT_TRUE(future1.blockResult());
    EXPECT_FALSE(future2.tryResult());
    d_eventLoop.post(
        bdlf::BindUtil::bind(&rmqamqp::SendChannel::processReceived,
                             mockUpdateSendChannel,
                             bsl::ref(bindOk)));
    EXPECT_TRUE(future2.blockResult());
}

class TracingProducerImplTests : public ProducerImplMaxOutstandingTests {
  public:
};

MATCHER_P(MessagePropertiesMatch, expected, "")
{
    return arg.properties() == expected;
}

TEST_P(TracingProducerImplTests, SendConfirmCallsTracing)
{
    // GIVEN
    bsl::shared_ptr<MockProducerTracing::MockContext> tracingContext(
        bsl::make_shared<MockProducerTracing::MockContext>());
    rmqt::Properties specialProperties = d_message.properties();
    specialProperties.headers          = bsl::make_shared<rmqt::FieldTable>();
    specialProperties.headers->insert(
        bsl::make_pair(bsl::string("special"), bsl::string("property")));

    bsl::shared_ptr<rmqa::ProducerImpl> producer(d_factory->create(
        1, d_exchange, d_mockSendChannel, d_threadPool, d_eventLoop));

    EXPECT_CALL(*d_mockSendChannel,
                publishMessage(MessagePropertiesMatch(specialProperties),
                               bsl::string("routingKey"),
                               _));
    EXPECT_CALL(
        *d_tracing,
        createAndTag(
            _, bsl::string("routingKey"), bsl::string("test-exchange"), _))
        .WillOnce(
            DoAll(SetArgPointee<0>(specialProperties), Return(tracingContext)));
    // WHEN
    producer->send(d_message, "routingKey", d_callback, d_timeout);

    const rmqt::ConfirmResponse confirmResponse(rmqt::ConfirmResponse::ACK);

    EXPECT_CALL(*tracingContext, response(confirmResponse)).WillOnce(Return());

    d_injectConfirm(d_message, d_exchange->name(), confirmResponse);

    d_threadPool.drain();
}

// We need to stick to INSTANTIATE_TEST_CASE_P for a while longer
// But we do want to build with -Werror in our CI
#pragma GCC diagnostic warning "-Wdeprecated-declarations"

INSTANTIATE_TEST_CASE_P(AllMembers,
                        ProducerImplTests,
                        Values(PRODUCER, TRACING_PRODUCER),
                        ProducerImplTests::PrintParamName());
INSTANTIATE_TEST_CASE_P(AllMembers,
                        ProducerImplConfirmTypeTests,
                        Values(PRODUCER, TRACING_PRODUCER),
                        ProducerImplTests::PrintParamName());
INSTANTIATE_TEST_CASE_P(AllMembers,
                        ProducerImplCallbackLifetimeTests,
                        Values(PRODUCER, TRACING_PRODUCER),
                        ProducerImplTests::PrintParamName());
INSTANTIATE_TEST_CASE_P(AllMembers,
                        ProducerImplMaxOutstandingTests,
                        Values(PRODUCER, TRACING_PRODUCER),
                        ProducerImplTests::PrintParamName());
INSTANTIATE_TEST_CASE_P(AllMembers,
                        ProducerImplUpdateTopology,
                        Values(PRODUCER, TRACING_PRODUCER),
                        ProducerImplTests::PrintParamName());

INSTANTIATE_TEST_CASE_P(AllMembers,
                        TracingProducerImplTests,
                        Values(TRACING_PRODUCER),
                        ProducerImplTests::PrintParamName());
