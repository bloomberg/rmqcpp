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

#include <rmqamqpt_exchangebind.h>

#include <rmqamqp_framer.h>
#include <rmqamqpt_exchangemethod.h>
#include <rmqamqpt_frame.h>
#include <rmqt_fieldvalue.h>

#include <bsl_cstddef.h>
#include <bsl_string.h>
#include <bsl_vector.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqamqpt;
using namespace rmqamqp;
using namespace ::testing;

TEST(Methods_Exchange_Bind, ExchangeBindEncodeDecode)
{
    rmqt::FieldTable ft;
    rmqt::FieldValue fv(bsl::string("String"));
    ft["value"] = fv;
    bsl::string sourceX("Source exchange");
    bsl::string destinationX("Destination exchange");
    bsl::string routingKey("Test key");
    bool noWait = true;

    ExchangeBind exchangeBindMethod(
        sourceX, destinationX, routingKey, noWait, ft);

    rmqamqp::Framer framer;

    rmqamqpt::Frame frame;
    rmqamqp::Framer::makeMethodFrame(
        &frame, 0, rmqamqpt::ExchangeMethod(exchangeBindMethod));

    uint16_t channel;
    rmqamqp::Message received;
    EXPECT_THAT(framer.appendFrame(&channel, &received, frame),
                Eq(rmqamqp::Framer::OK));

    ASSERT_TRUE(received.is<rmqamqpt::Method>());
    const rmqamqpt::Method& m = received.the<rmqamqpt::Method>();

    ASSERT_TRUE(m.is<rmqamqpt::ExchangeMethod>());
    const rmqamqpt::ExchangeMethod& qm = m.the<rmqamqpt::ExchangeMethod>();

    ASSERT_TRUE(qm.is<rmqamqpt::ExchangeBind>());
    EXPECT_EQ(exchangeBindMethod, qm.the<rmqamqpt::ExchangeBind>());
}

TEST(Methods_ExchangeBind, NoWaitIsReadFromBitZeroOnly)
{
    const uint8_t cases[][2] = {{0x00, 0}, {0x01, 1}, {0x02, 0}, {0xFF, 1}};

    for (bsl::size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        // reserved short, three empty short strings, the packed bit octet,
        // then an empty field table
        const uint8_t data[] = {
            0x00, 0x00, 0x00, 0x00, 0x00, cases[i][0], 0x00, 0x00, 0x00, 0x00};

        rmqamqpt::ExchangeBind bind;
        EXPECT_TRUE(rmqamqpt::ExchangeBind::decode(&bind, data, sizeof(data)));
        EXPECT_EQ(static_cast<int>(bind.noWait()), cases[i][1])
            << "octet: " << static_cast<int>(cases[i][0]);
    }
}
