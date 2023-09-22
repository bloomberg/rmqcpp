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

#include <rmqt_exchange.h>

#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(ExchangeTests, DefaultExchangeCheck)
{
    rmqt::Exchange defaultExch(
        "", false, rmqt::ExchangeType::DIRECT, false, true);

    EXPECT_TRUE(defaultExch.isDefault());
}

TEST(ExchangeTests, DefaultExchangeCheckDifferentName)
{
    rmqt::Exchange defaultExch("not-default");

    EXPECT_FALSE(defaultExch.isDefault());
}

TEST(Exchange, EqualsOperator)
{
    rmqt::Exchange a("foo", true, rmqt::ExchangeType::DIRECT);
    rmqt::Exchange b("foo", true, rmqt::ExchangeType::FANOUT);
    ASSERT_EQ(a, a);
    ASSERT_NE(a, b);
}

TEST(ExchangeUtilTests, ValidateExchangeName)
{
    EXPECT_TRUE(rmqt::ExchangeUtil::validateName("exchange-name"));
}

TEST(ExchangeUtilTests, ValidateIllegalExchangeName)
{
    EXPECT_FALSE(rmqt::ExchangeUtil::validateName("Illegal name @"));
}
