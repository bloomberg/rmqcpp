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

#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqa;
using namespace ::testing;

namespace {
bool alwaysHealthy() { return true; }
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
