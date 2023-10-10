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

using namespace BloombergLP;
using namespace ::testing;

TEST(Methods_ConnectionStartOk, StartOkEncodeDecode)
{
    rmqt::FieldTable ft;
    rmqt::FieldValue fv(bsl::string("String"));
    ft["foo"] = fv;
    bsl::string mechanism("PLAIN");
    bsl::string locale("en-US");
    bsl::string response("guest");
    rmqamqpt::ConnectionStartOk startOkMethod(ft, mechanism, response, locale);

    rmqamqp::Framer framer;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 0, rmqamqpt::ConnectionMethod(startOkMethod));

    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    EXPECT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    EXPECT_TRUE(m.is<rmqamqpt::ConnectionMethod>());
    const rmqamqpt::ConnectionMethod& cm = m.the<rmqamqpt::ConnectionMethod>();

    EXPECT_TRUE(cm.is<rmqamqpt::ConnectionStartOk>());
    EXPECT_EQ(startOkMethod, cm.the<rmqamqpt::ConnectionStartOk>());
}
