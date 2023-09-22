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

#include <rmqtestmocks_mockconnection.h>
#include <rmqtestmocks_mockconsumer.h>
#include <rmqtestmocks_mockmessageguard.h>
#include <rmqtestmocks_mockproducer.h>
#include <rmqtestmocks_mockrabbitcontext.h>
#include <rmqtestmocks_mockvhost.h>

#include <gtest/gtest.h>

using namespace BloombergLP;

TEST(MockTests, InitialiseMocks)
{
    // Checks none are abstract classes (forgot to update mock objects?)

    rmqtestmocks::MockConsumer mockConsumer;
    rmqtestmocks::MockRabbitContext mockContext;
    rmqtestmocks::MockVHost mockVhost;
    rmqtestmocks::MockConnection mockConnection;
    rmqtestmocks::MockProducer mockProducer;
    rmqtestmocks::MockMessageGuard guard;

    // Intentionally unused
    (void)mockConsumer;
    (void)mockContext;
    (void)mockVhost;
    (void)mockConnection;
    (void)mockProducer;
}
