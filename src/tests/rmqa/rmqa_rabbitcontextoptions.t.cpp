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

#include <rmqa_rabbitcontextoptions.h>

#include <rmqt_hosthealthconfig.h>

#include <bsls_asserttest.h>
#include <bsls_timeinterval.h>

#include <bsl_optional.h>

#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqa;
using namespace ::testing;

namespace {
bool alwaysHealthy() { return true; }

/// `bsls_asserttest`'s `BSLS_ASSERTTEST_ASSERT_{PASS,FAIL}` macros report their
/// outcome by calling a driver-supplied `ASSERT(bool)` (which its header allows
/// to be a function, not just a macro). Route that into a gtest expectation.
void ASSERT(bool result) { EXPECT_TRUE(result); }
} // namespace

TEST(RabbitContextOptions, Constructs) { rmqa::RabbitContextOptions t; }
TEST(RabbitContextOptions, Defaults)
{
    rmqa::RabbitContextOptions t;
    EXPECT_FALSE(t.metricPublisher());
    EXPECT_FALSE(t.threadpool());
    EXPECT_FALSE(t.hostHealthConfig().has_value());
    t.errorCallback()("heres an error", -1);
}

TEST(RabbitContextOptions, SetHostHealthConfig)
{
    rmqa::RabbitContextOptions options;

    // Initially not set
    EXPECT_FALSE(options.hostHealthConfig().has_value());

    // Create a health checker function
    rmqt::HostHealthConfig config(alwaysHealthy);

    // Set host health config
    options.setHostHealthConfig(config);

    // Now it should be set
    EXPECT_TRUE(options.hostHealthConfig().has_value());
}

TEST(RabbitContextOptions, SetConnectionEstablishmentTimeoutAcceptsValid)
{
    rmqa::RabbitContextOptions options;
    bsls::AssertTestHandlerGuard guard;

    // Unset is allowed (falls back to the library default).
    BSLS_ASSERTTEST_ASSERT_PASS(
        options.setConnectionEstablishmentTimeout(bsl::nullopt));

    // A whole-second positive value is allowed.
    BSLS_ASSERTTEST_ASSERT_PASS(
        options.setConnectionEstablishmentTimeout(bsls::TimeInterval(5, 0)));
    EXPECT_EQ(options.connectionEstablishmentTimeout(),
              bsls::TimeInterval(5, 0));

    // A sub-second value must be accepted (500ms), as must the 1ms granularity
    // floor the hung timer supports.
    BSLS_ASSERTTEST_ASSERT_PASS(options.setConnectionEstablishmentTimeout(
        bsls::TimeInterval(0, 500 * 1000 * 1000)));
    EXPECT_EQ(options.connectionEstablishmentTimeout(),
              bsls::TimeInterval(0, 500 * 1000 * 1000));

    BSLS_ASSERTTEST_ASSERT_PASS(options.setConnectionEstablishmentTimeout(
        bsls::TimeInterval(0, 1000 * 1000))); // exactly 1ms
}

TEST(RabbitContextOptions, SetConnectionEstablishmentTimeoutRejectsTooSmall)
{
    rmqa::RabbitContextOptions options;
    bsls::AssertTestHandlerGuard guard;

    // Zero would expire the establishment bound immediately.
    BSLS_ASSERTTEST_ASSERT_FAIL(
        options.setConnectionEstablishmentTimeout(bsls::TimeInterval(0, 0)));

    // Negative is likewise invalid.
    BSLS_ASSERTTEST_ASSERT_FAIL(
        options.setConnectionEstablishmentTimeout(bsls::TimeInterval(-1, 0)));

    // A positive but sub-millisecond interval (500us) rounds down to 0ms in the
    // hung timer, so it is rejected too.
    BSLS_ASSERTTEST_ASSERT_FAIL(options.setConnectionEstablishmentTimeout(
        bsls::TimeInterval(0, 500 * 1000)));
}
