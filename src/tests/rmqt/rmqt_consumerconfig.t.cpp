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

#include <rmqt_consumerconfig.h>

#include <rmqt_properties.h>

#include <bdlmt_threadpool.h>
#include <bslmt_threadattributes.h>

#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

TEST(ConsumerConfig, DefaultConstructorValues)
{
    rmqt::ConsumerConfig config(
        "tag"); // consumerTag default value is randomly generated

    EXPECT_EQ(config.consumerTag(), "tag");
    EXPECT_EQ(config.prefetchCount(),
              rmqt::ConsumerConfig::s_defaultPrefetchCount);
    EXPECT_EQ(config.threadpool(), static_cast<bdlmt::ThreadPool*>(NULL));
    EXPECT_EQ(config.exclusiveFlag(), rmqt::Exclusive::OFF);
    EXPECT_FALSE(config.consumerPriority());
}

TEST(ConsumerConfig, Constructor)
{
    bdlmt::ThreadPool threadPool(bslmt::ThreadAttributes(), 1, 1, 60000);
    rmqt::ConsumerConfig config(
        "tag", 100, &threadPool, rmqt::Exclusive::ON, 5);

    EXPECT_EQ(config.consumerTag(), "tag");
    EXPECT_EQ(config.prefetchCount(), 100);
    EXPECT_EQ(config.threadpool(), &threadPool);
    EXPECT_EQ(config.exclusiveFlag(), rmqt::Exclusive::ON);
    EXPECT_EQ(config.consumerPriority(), 5);
}

TEST(ConsumerConfig, SetConsumerPriority)
{
    rmqt::ConsumerConfig config;
    int64_t consumerPriority = 5;

    EXPECT_NE(config.consumerPriority(), consumerPriority);

    config.setConsumerPriority(consumerPriority);

    EXPECT_TRUE(config.consumerPriority());
    EXPECT_EQ(config.consumerPriority(), 5);
}
