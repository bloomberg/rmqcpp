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

#include <rmqamqp_messagestore.h>

#include <rmqt_message.h>

#include <bdlt_currenttime.h>
#include <bdlt_epochutil.h>
#include <bsl_iostream.h>

#include <bsl_utility.h>
#include <bsl_vector.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(MessageStore, InitialiseEmpty)
{
    rmqamqp::MessageStore<rmqt::Message> msgStore;
    EXPECT_THAT(msgStore.count(), Eq(0));
    EXPECT_THAT(msgStore.lifetimeId(), Eq(0));
}

TEST(MessageStore, InsertTwoCheckCounts)
{
    rmqamqp::MessageStore<rmqt::Message> msgStore;

    uint64_t tag1(1), tag2(2);
    rmqt::Message msg1;
    rmqt::Message msg2;

    EXPECT_EQ(msgStore.insert(tag1, msg1), true);
    EXPECT_EQ(msgStore.insert(tag2, msg2), true);
    EXPECT_EQ(msgStore.count(), 2);
    EXPECT_EQ(msgStore.latestTagTilNow(), tag2);
    EXPECT_EQ(msgStore.latestTagInStore(), tag2);
    EXPECT_EQ(msgStore.oldestTagInStore(), tag1);
}

TEST(MessageStore, InsertLookupByTagSameId)
{
    rmqamqp::MessageStore<rmqt::Message> msgStore;

    uint64_t tag1(1);
    rmqt::Message msg1;
    EXPECT_EQ(msgStore.insert(tag1, msg1), true);

    rmqt::Message msg;
    EXPECT_EQ(msgStore.lookup(tag1, &msg), true);
    EXPECT_EQ(msg.guid(), msg1.guid());
}

TEST(MessageStore, InsertLookupByIdSame)
{
    rmqamqp::MessageStore<rmqt::Message> msgStore;

    uint64_t tag1(1);
    rmqt::Message msg1;
    EXPECT_EQ(msgStore.insert(tag1, msg1), true);

    rmqt::Message msg;
    uint64_t deliveryTag;
    EXPECT_EQ(msgStore.lookup(msg1.guid(), &msg, &deliveryTag), true);
    EXPECT_EQ(msg.guid(), msg1.guid());
    EXPECT_EQ(tag1, deliveryTag);
}

TEST(MessageStore, CannotLookupOrRemoveUnknownTag)
{
    rmqamqp::MessageStore<rmqt::Message> msgStore;

    uint64_t tag1(1);
    rmqt::Message msg1;

    rmqt::Message msg;
    uint64_t deliveryTag;
    bdlt::Datetime insertTime;
    EXPECT_EQ(msgStore.lookup(tag1, &msg), false);
    EXPECT_EQ(msgStore.lookup(msg1.guid(), &msg, &deliveryTag), false);

    EXPECT_EQ(msgStore.remove(tag1, &msg, &insertTime), false);
}

TEST(MessageStore, CheckCountsAfterRemoval)
{
    rmqamqp::MessageStore<rmqt::Message> msgStore;

    uint64_t tag1(1), tag2(2), tag3(3);
    rmqt::Message msg1;
    rmqt::Message msg2;
    rmqt::Message msg3;
    rmqt::Message removed;
    bdlt::Datetime insertTime;
    EXPECT_EQ(msgStore.insert(tag1, msg1), true);
    EXPECT_EQ(msgStore.insert(tag2, msg2), true);
    EXPECT_EQ(msgStore.remove(tag1, &removed, &insertTime), true);
    EXPECT_EQ(msgStore.insert(tag3, msg3), true);

    EXPECT_EQ(msgStore.count(), 2);
    EXPECT_EQ(msgStore.latestTagTilNow(), tag3);
    EXPECT_EQ(msgStore.latestTagInStore(), tag3);
    EXPECT_EQ(msgStore.oldestTagInStore(), tag2);
}

TEST(MessageStore, RemoveUntil)
{
    rmqamqp::MessageStore<rmqt::Message> msgStore;

    uint64_t tag1(2), tag2(3), tag3(4);
    rmqt::Message msg1;
    rmqt::Message msg2;
    rmqt::Message msg3;
    EXPECT_EQ(msgStore.insert(tag1, msg1), true);
    EXPECT_EQ(msgStore.insert(tag2, msg2), true);
    EXPECT_EQ(msgStore.insert(tag3, msg3), true);

    bsl::vector<bsl::pair<uint64_t, bsl::pair<rmqt::Message, bdlt::Datetime> > >
        removedMessages;
    removedMessages = msgStore.removeUntil(tag2);
    EXPECT_EQ(removedMessages.size(), 2);
    EXPECT_EQ(removedMessages[0].first, tag1);
    EXPECT_EQ(removedMessages[1].first, tag2);

    EXPECT_EQ(msgStore.count(), 1);
    EXPECT_EQ(msgStore.latestTagTilNow(), tag3);
    EXPECT_EQ(msgStore.latestTagInStore(), tag3);
    EXPECT_EQ(msgStore.oldestTagInStore(), tag3);
}

static bsls::TimeInterval s_time(0);

static bsls::TimeInterval time() { return s_time; }

TEST(MessageStore, GetMessagesOlderThan)
{
    bdlt::Datetime testTime = bdlt::CurrentTime::utc();
    s_time                  = bsls::TimeInterval(
        (testTime - bdlt::EpochUtil::epoch()).totalSecondsAsDouble());

    bdlt::CurrentTime::CurrentTimeCallback prev =
        bdlt::CurrentTime::setCurrentTimeCallback(time);

    rmqamqp::MessageStore<rmqt::Message> msgStore;

    uint64_t tag1(1), tag2(2);
    rmqt::Message msg1;
    rmqt::Message msg2;

    // insert at time 0
    EXPECT_EQ(msgStore.insert(tag1, msg1), true);

    s_time += bsls::TimeInterval(10);

    // insert at time 10
    EXPECT_EQ(msgStore.insert(tag2, msg2), true);

    // Check at time 20
    s_time += bsls::TimeInterval(10);

    testTime.addSeconds(9);

    rmqamqp::MessageStore<rmqt::Message>::MessageList messages =
        msgStore.getMessagesOlderThan(testTime);

    EXPECT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].second.first, msg1);

    bdlt::CurrentTime::setCurrentTimeCallback(prev);
}

TEST(MessageStore, Swap)
{
    rmqamqp::MessageStore<rmqt::Message> msgStore;
    EXPECT_EQ(msgStore.count(), 0);
    EXPECT_EQ(msgStore.lifetimeId(), 0);

    uint64_t tag1(1);
    rmqt::Message msg1;

    EXPECT_EQ(msgStore.insert(tag1, msg1), true);
    EXPECT_EQ(msgStore.count(), 1);
    EXPECT_EQ(msgStore.lifetimeId(), 0);

    rmqamqp::MessageStore<rmqt::Message> msgStore2;

    msgStore.swap(msgStore2);

    EXPECT_EQ(msgStore.lifetimeId(), 1);

    EXPECT_EQ(msgStore.count(), 0);
    EXPECT_EQ(msgStore2.count(), 1);
}
