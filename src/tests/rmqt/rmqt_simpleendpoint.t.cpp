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

#include <gtest/gtest.h>

#include <rmqt_simpleendpoint.h>

using namespace BloombergLP;
using namespace rmqt;

TEST(SimpleEndpoint, DefaultPort)
{
    SimpleEndpoint e("test.example.com", "rmq-testing");
    ASSERT_EQ("amqp://test.example.com:5672/rmq-testing", e.formatAddress());
}

TEST(SimpleEndpoint, SelectedPort)
{
    SimpleEndpoint e("test.example.com", "rmq-testing", 12345);
    ASSERT_EQ("amqp://test.example.com:12345/rmq-testing", e.formatAddress());
}
