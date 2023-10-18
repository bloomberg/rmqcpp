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

#include <rmqamqpt_queuedeleteok.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_queuemethod.h>

#include <rmqamqpt_frame.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqpt;
using namespace rmqamqp;
using namespace ::testing;

TEST(Methods_Queue_Delete_Ok, QueueDeleteOkEncodeDecode)
{
    QueueDeleteOk method(42);

    rmqamqp::Framer framer;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(&frame, 0, rmqamqpt::QueueMethod(method));

    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    ASSERT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    ASSERT_TRUE(m.is<rmqamqpt::QueueMethod>());
    const rmqamqpt::QueueMethod& qm = m.the<rmqamqpt::QueueMethod>();

    ASSERT_TRUE(qm.is<rmqamqpt::QueueDeleteOk>());
    EXPECT_EQ(method, qm.the<rmqamqpt::QueueDeleteOk>());
    EXPECT_EQ(qm.the<rmqamqpt::QueueDeleteOk>().messageCount(), 42);
}
