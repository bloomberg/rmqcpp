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

#include <rmqamqpt_queuedelete.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_frame.h>
#include <rmqamqpt_queuemethod.h>

#include <bsl_string.h>
#include <bsl_vector.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(Methods_Queue_Delete, QueueDeleteEncodeDecode)
{
    bsl::string queueName("Test queue");

    for (int ifUnused = 0; ifUnused <= 1; ifUnused++) {
        for (int ifEmpty = 0; ifEmpty <= 1; ifEmpty++) {
            for (int noWait = 0; noWait <= 1; noWait++) {
                rmqamqpt::QueueDelete queueDeleteMethod(
                    queueName, ifUnused, ifEmpty, noWait);

                rmqamqp::Framer framer;

                rmqamqpt::Frame frame;
                rmqamqp::Framer::makeMethodFrame(
                    &frame, 0, rmqamqpt::QueueMethod(queueDeleteMethod));

                uint16_t channel;
                rmqamqp::Message received;
                EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                            Eq(rmqamqp::Framer::OK));

                ASSERT_TRUE(received.is<rmqamqpt::Method>());
                const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

                ASSERT_TRUE(m.is<rmqamqpt::QueueMethod>());
                const rmqamqpt::QueueMethod& qm =
                    m.the<rmqamqpt::QueueMethod>();

                ASSERT_TRUE(qm.is<rmqamqpt::QueueDelete>());
                EXPECT_EQ(queueDeleteMethod, qm.the<rmqamqpt::QueueDelete>());
                EXPECT_EQ(qm.the<rmqamqpt::QueueDelete>().name(), "Test queue");
                EXPECT_EQ(qm.the<rmqamqpt::QueueDelete>().ifUnused(), ifUnused);
                EXPECT_EQ(qm.the<rmqamqpt::QueueDelete>().ifEmpty(), ifEmpty);
                EXPECT_EQ(qm.the<rmqamqpt::QueueDelete>().noWait(), noWait);
            }
        }
    }
}
