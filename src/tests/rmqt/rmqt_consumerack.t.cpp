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

#include <rmqt_consumerack.h>

#include <rmqt_envelope.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_sstream.h>
#include <bsl_string.h>

using namespace BloombergLP;
using namespace ::testing;

namespace {

bsl::string toString(rmqt::ConsumerAck::Type type)
{
    bsl::ostringstream os;
    os << type;
    return os.str();
}

} // namespace

// The values are the stable part of this enum: they go in no wire frame, but
// the deprecated aliases only remain compatible while they keep the value
// their old name had.
TEST(ConsumerAckTests, ValuesAreFixed)
{
    EXPECT_EQ(rmqt::ConsumerAck::ACK, 0);
    EXPECT_EQ(rmqt::ConsumerAck::NACK_REQUEUE_FALSE, 1);
    EXPECT_EQ(rmqt::ConsumerAck::NACK_REQUEUE_TRUE, 2);
    EXPECT_EQ(rmqt::ConsumerAck::REJECT_REQUEUE_FALSE, 3);
    EXPECT_EQ(rmqt::ConsumerAck::REJECT_REQUEUE_TRUE, 4);
}

// REJECT and REQUEUE predate support for basic.reject and both send
// basic.nack. They have to keep meaning exactly what they meant before.
TEST(ConsumerAckTests, DeprecatedAliasesAreUnchanged)
{
    EXPECT_EQ(rmqt::ConsumerAck::REJECT, rmqt::ConsumerAck::NACK_REQUEUE_FALSE);
    EXPECT_EQ(rmqt::ConsumerAck::REQUEUE, rmqt::ConsumerAck::NACK_REQUEUE_TRUE);
}

TEST(ConsumerAckTests, UsesBasicReject)
{
    EXPECT_FALSE(rmqt::ConsumerAck::usesBasicReject(rmqt::ConsumerAck::ACK));
    EXPECT_FALSE(rmqt::ConsumerAck::usesBasicReject(
        rmqt::ConsumerAck::NACK_REQUEUE_FALSE));
    EXPECT_FALSE(rmqt::ConsumerAck::usesBasicReject(
        rmqt::ConsumerAck::NACK_REQUEUE_TRUE));
    EXPECT_TRUE(rmqt::ConsumerAck::usesBasicReject(
        rmqt::ConsumerAck::REJECT_REQUEUE_FALSE));
    EXPECT_TRUE(rmqt::ConsumerAck::usesBasicReject(
        rmqt::ConsumerAck::REJECT_REQUEUE_TRUE));
}

// The deprecated REJECT sends basic.nack, so it must not be reported as a
// basic.reject however much its name suggests otherwise.
TEST(ConsumerAckTests, DeprecatedRejectDoesNotUseBasicReject)
{
    EXPECT_FALSE(rmqt::ConsumerAck::usesBasicReject(rmqt::ConsumerAck::REJECT));
}

// basic.ack and basic.nack carry a `multiple` flag, basic.reject does not.
// Batching a reject would resolve only the highest delivery tag in the batch
// and silently leave the rest outstanding.
TEST(ConsumerAckTests, SupportsMultiple)
{
    EXPECT_TRUE(rmqt::ConsumerAck::supportsMultiple(rmqt::ConsumerAck::ACK));
    EXPECT_TRUE(rmqt::ConsumerAck::supportsMultiple(
        rmqt::ConsumerAck::NACK_REQUEUE_FALSE));
    EXPECT_TRUE(rmqt::ConsumerAck::supportsMultiple(
        rmqt::ConsumerAck::NACK_REQUEUE_TRUE));
    EXPECT_FALSE(rmqt::ConsumerAck::supportsMultiple(
        rmqt::ConsumerAck::REJECT_REQUEUE_FALSE));
    EXPECT_FALSE(rmqt::ConsumerAck::supportsMultiple(
        rmqt::ConsumerAck::REJECT_REQUEUE_TRUE));
}

TEST(ConsumerAckTests, StreamOut)
{
    EXPECT_EQ(toString(rmqt::ConsumerAck::ACK), "ACK");
    EXPECT_EQ(toString(rmqt::ConsumerAck::NACK_REQUEUE_FALSE),
              "NACK_REQUEUE_FALSE");
    EXPECT_EQ(toString(rmqt::ConsumerAck::NACK_REQUEUE_TRUE),
              "NACK_REQUEUE_TRUE");
    EXPECT_EQ(toString(rmqt::ConsumerAck::REJECT_REQUEUE_FALSE),
              "REJECT_REQUEUE_FALSE");
    EXPECT_EQ(toString(rmqt::ConsumerAck::REJECT_REQUEUE_TRUE),
              "REJECT_REQUEUE_TRUE");
}

TEST(ConsumerAckTests, StreamOutUnknown)
{
    EXPECT_EQ(toString(static_cast<rmqt::ConsumerAck::Type>(99)),
              "UNKNOWN(99)");
}

TEST(ConsumerAckTests, CheckMembers)
{
    const rmqt::Envelope envelope(
        1, 2, "consumerTag", "exchange", "routingKey", true);
    const rmqt::ConsumerAck ack(envelope,
                                rmqt::ConsumerAck::REJECT_REQUEUE_TRUE);

    EXPECT_EQ(ack.type(), rmqt::ConsumerAck::REJECT_REQUEUE_TRUE);
    EXPECT_EQ(ack.envelope().deliveryTag(), 1u);
}
