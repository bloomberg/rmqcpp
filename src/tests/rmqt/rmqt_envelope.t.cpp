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

#include <rmqt_envelope.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_sstream.h>
#include <bsl_string.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(EnvelopeTests, CheckMembers)
{
    rmqt::Envelope env(1, 2, "consumerTag", "exchange", "routingKey", true);

    EXPECT_THAT(env.deliveryTag(), Eq(1));
    EXPECT_THAT(env.channelLifetimeId(), Eq(2));
    EXPECT_THAT(env.consumerTag(), Eq("consumerTag"));
    EXPECT_THAT(env.exchange(), Eq("exchange"));
    EXPECT_THAT(env.routingKey(), Eq("routingKey"));
    EXPECT_THAT(env.redelivered(), Eq(true));
}

TEST(EnvelopeTests, StreamOut)
{
    bsl::ostringstream oss;
    rmqt::Envelope env(
        3812, 99, "myConsumerTag", "myExchange", "myRoutingKey", true);
    oss << env;
    bsl::string output = oss.str();
    EXPECT_THAT(output, HasSubstr("3812"));
    EXPECT_THAT(output, HasSubstr("99"));
    EXPECT_THAT(output, HasSubstr("myConsumerTag"));
    EXPECT_THAT(output, HasSubstr("myExchange"));
    EXPECT_THAT(output, HasSubstr("myRoutingKey"));
    EXPECT_THAT(output, HasSubstr("true"));
}
