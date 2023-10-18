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

#include <rmqamqpt_connectionstart.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_frame.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdint.h>
#include <bsl_string.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(ConnectionStart, Start_Breathing) { rmqamqpt::ConnectionStart s; }

TEST(ConnectionStart, OpenEncodeDecode)
{
    rmqt::FieldTable ft;
    rmqt::FieldValue fv(bsl::string("String"));
    ft["foo"] = fv;

    bsl::vector<bsl::string> mechanisms;
    mechanisms.push_back("PLAIN");
    mechanisms.push_back("AMQPLAIN");
    bsl::vector<bsl::string> locales;
    locales.push_back("en-US");
    rmqamqpt::ConnectionStart startMethod(0, 9, ft, mechanisms, locales);

    rmqamqp::Framer framer;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 0, rmqamqpt::ConnectionMethod(startMethod));

    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    ASSERT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    ASSERT_TRUE(m.is<rmqamqpt::ConnectionMethod>());
    const rmqamqpt::ConnectionMethod& cm = m.the<rmqamqpt::ConnectionMethod>();

    ASSERT_TRUE(cm.is<rmqamqpt::ConnectionStart>());
    EXPECT_EQ(startMethod, cm.the<rmqamqpt::ConnectionStart>());
}
