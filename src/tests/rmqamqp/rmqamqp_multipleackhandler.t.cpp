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

#include <rmqamqp_multipleackhandler.h>

#include <bdlf_bind.h>

#include <bsl_vector.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqp;
using namespace ::testing;
using namespace bdlf::PlaceHolders;

namespace {
class Callbacks {
  public:
    virtual void ack(uint64_t, bool)        = 0;
    virtual void nack(uint64_t, bool, bool) = 0;
    virtual void reject(uint64_t, bool)     = 0;
};

class MockCallbacks : public Callbacks {
  public:
    MOCK_METHOD2(ack, void(uint64_t, bool));
    MOCK_METHOD3(nack, void(uint64_t, bool, bool));
    MOCK_METHOD2(reject, void(uint64_t, bool));
};

Sequence seq;
} // namespace

class MultipleAckHandlerTests : public ::testing::Test {
  public:
    MockCallbacks d_cb;
    MultipleAckHandler d_handler;
    bsl::vector<rmqt::ConsumerAck> d_acks;

    MultipleAckHandlerTests()
    : d_handler(bdlf::BindUtil::bind(&Callbacks::ack, &d_cb, _1, _2),
                bdlf::BindUtil::bind(&Callbacks::nack, &d_cb, _1, _2, _3),
                bdlf::BindUtil::bind(&Callbacks::reject, &d_cb, _1, _2))
    {
    }

    void ack(uint64_t dt)
    {
        d_acks.push_back(rmqt::ConsumerAck(
            rmqt::Envelope(
                dt, 0, "consumerTag", "exchange", "routing-key", false),
            rmqt::ConsumerAck::ACK));
    }

    void nack(uint64_t dt, bool requeue)
    {
        d_acks.push_back(rmqt::ConsumerAck(
            rmqt::Envelope(
                dt, 0, "consumerTag", "exchange", "routing-key", false),
            requeue ? rmqt::ConsumerAck::NACK_REQUEUE_TRUE
                    : rmqt::ConsumerAck::NACK_REQUEUE_FALSE));
    }

    void reject(uint64_t dt, bool requeue)
    {
        d_acks.push_back(rmqt::ConsumerAck(
            rmqt::Envelope(
                dt, 0, "consumerTag", "exchange", "routing-key", false),
            requeue ? rmqt::ConsumerAck::REJECT_REQUEUE_TRUE
                    : rmqt::ConsumerAck::REJECT_REQUEUE_FALSE));
    }

    void expectAck(uint64_t tag, bool multiple)
    {
        EXPECT_CALL(d_cb, ack(tag, multiple)).InSequence(seq);
    }

    void expectNack(uint64_t tag, bool requeue, bool multiple)
    {
        EXPECT_CALL(d_cb, nack(tag, requeue, multiple)).InSequence(seq);
    }

    void expectReject(uint64_t tag, bool requeue)
    {
        EXPECT_CALL(d_cb, reject(tag, requeue)).InSequence(seq);
    }

    void process()
    {
        d_handler.process(d_acks);
        d_acks.clear();
    }
};

TEST_F(MultipleAckHandlerTests, Breathing) {}

TEST_F(MultipleAckHandlerTests, SingleAck)
{
    ack(1);
    expectAck(1, false);
    process();
}

TEST_F(MultipleAckHandlerTests, SingleRequeue)
{
    nack(1, true);
    expectNack(1, true, false);
    process();
}

TEST_F(MultipleAckHandlerTests, SingleNackDiscard)
{
    nack(1, false);
    expectNack(1, false, false);
    process();
}

TEST_F(MultipleAckHandlerTests, SingleRejectRequeue)
{
    reject(1, true);
    expectReject(1, true);
    process();
}

TEST_F(MultipleAckHandlerTests, SingleRejectDiscard)
{
    reject(1, false);
    expectReject(1, false);
    process();
}

TEST_F(MultipleAckHandlerTests, MultiAck)
{
    ack(2);
    ack(3);
    ack(5);
    ack(4);
    ack(1);
    expectAck(5, true);
    process();
}

TEST_F(MultipleAckHandlerTests, DelayedAcks)
{
    ack(3);
    ack(1);
    expectAck(1, false);
    expectAck(3, false);
    process();

    ack(2);
    ack(5);
    ack(4);
    expectAck(2, false);
    expectAck(5, true);
    process();

    ack(7);
    expectAck(7, false);
    process();

    ack(6);
    expectAck(6, false);
    process();
}

TEST_F(MultipleAckHandlerTests, MissingAckNotAcked)
{
    ack(1);
    ack(2);
    ack(3);
    expectAck(3, true);
    process();

    ack(5);
    ack(6);
    ack(7);
    expectAck(5, false);
    expectAck(6, false);
    expectAck(7, false);
    process();
}

TEST_F(MultipleAckHandlerTests, RequeuedMessage)
{
    ack(2);
    nack(3, true);
    ack(5);
    ack(4);
    ack(1);
    expectAck(2, true);
    expectNack(3, true, false);
    expectAck(5, true);
    process();
}

TEST_F(MultipleAckHandlerTests, DifferentTypes)
{
    ack(1);
    ack(2);
    nack(3, true);
    nack(4, false);
    nack(5, false);
    ack(6);
    expectAck(2, true);
    expectNack(3, true, false);
    expectNack(5, false, true);
    expectAck(6, false);
    process();
}

// basic.reject has no `multiple` field, so a run of consecutive rejects must
// never be collapsed into one frame. Batching them would resolve only the
// highest delivery tag and silently leak the rest.
TEST_F(MultipleAckHandlerTests, ConsecutiveRejectsAreNotBatched)
{
    reject(1, true);
    reject(2, true);
    reject(3, true);
    expectReject(1, true);
    expectReject(2, true);
    expectReject(3, true);
    process();
}

TEST_F(MultipleAckHandlerTests, ConsecutiveRejectDiscardsAreNotBatched)
{
    reject(1, false);
    reject(2, false);
    reject(3, false);
    expectReject(1, false);
    expectReject(2, false);
    expectReject(3, false);
    process();
}

// A reject in the middle of a contiguous run must close the pending ack batch
// and must not stop the acks after it from batching again.
TEST_F(MultipleAckHandlerTests, RejectInterleavedWithAcks)
{
    ack(1);
    ack(2);
    reject(3, true);
    ack(4);
    ack(5);
    expectAck(2, true);
    expectReject(3, true);
    expectAck(5, true);
    process();
}

TEST_F(MultipleAckHandlerTests, RejectsAndNacksAreNotConflated)
{
    nack(1, true);
    reject(2, true);
    nack(3, false);
    reject(4, false);
    expectNack(1, true, false);
    expectReject(2, true);
    expectNack(3, false, false);
    expectReject(4, false);
    process();
}

// All three resolutions in one contiguous run. Each batchable type batches
// only with its own kind, a reject always stands alone, and a reject in the
// middle does not stop the run after it batching again.
TEST_F(MultipleAckHandlerTests, AcksNacksAndRejectsMixed)
{
    ack(1);
    ack(2);
    nack(3, true);
    nack(4, true);
    reject(5, true);
    reject(6, false);
    nack(7, false);
    ack(8);
    ack(9);

    expectAck(2, true);        // 1-2 batched
    expectNack(4, true, true); // 3-4 batched, different type closed the ack
    expectReject(5, true);     // rejects never batch, not even with each
    expectReject(6, false);    // other
    expectNack(7, false, false);
    expectAck(9, true); // 8-9 batch again after the rejects
    process();
}
