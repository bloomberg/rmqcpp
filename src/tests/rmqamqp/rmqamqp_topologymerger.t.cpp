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

#include <rmqamqp_topologymerger.h>

#include <bsl_memory.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

class TopologyMergerTests : public Test {
  public:
    bsl::shared_ptr<rmqt::Exchange> exchange;
    bsl::shared_ptr<rmqt::Queue> queue;
    TopologyMergerTests()
    : exchange(bsl::make_shared<rmqt::Exchange>("exchange"))
    , queue(bsl::make_shared<rmqt::Queue>("queue"))
    {
    }
    void SetUp() BSLS_KEYWORD_OVERRIDE {}
};

TEST_F(TopologyMergerTests, mergeWithEmptyTopology)
{
    rmqt::Topology emptyTopology;
    rmqt::TopologyUpdate topologyUpdate;

    rmqt::FieldTable args;
    bsl::shared_ptr<rmqt::QueueBinding> queueBinding =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, "bindKey", args);
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBinding));
    rmqamqp::TopologyMerger::merge(emptyTopology, topologyUpdate);
    EXPECT_EQ(1, emptyTopology.queueBindings.size());
    EXPECT_EQ(exchange, emptyTopology.queueBindings[0]->exchange().lock());
    EXPECT_EQ(queue, emptyTopology.queueBindings[0]->queue().lock());
}

TEST_F(TopologyMergerTests, mergeWithNonEmptyTopology)
{
    rmqt::Topology topology;
    rmqt::TopologyUpdate topologyUpdate;

    rmqt::FieldTable args;
    bsl::shared_ptr<rmqt::QueueBinding> queueBinding =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, "bindKey", args);
    topology.queueBindings.push_back(queueBinding);
    bsl::shared_ptr<rmqt::QueueBinding> queueBinding2 =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, "bindKey2", args);
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBinding2));
    rmqamqp::TopologyMerger::merge(topology, topologyUpdate);
    EXPECT_EQ(2, topology.queueBindings.size());
    EXPECT_EQ("bindKey", topology.queueBindings[0]->bindingKey());
    EXPECT_EQ("bindKey2", topology.queueBindings[1]->bindingKey());
}

TEST_F(TopologyMergerTests, doNotAddDuplicateBind)
{
    rmqt::Topology topology;
    rmqt::TopologyUpdate topologyUpdate;

    rmqt::FieldTable args;
    bsl::shared_ptr<rmqt::QueueBinding> queueBinding =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, "bindKey", args);
    topology.queueBindings.push_back(queueBinding);
    bsl::shared_ptr<rmqt::QueueBinding> queueBinding2 =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, "bindKey", args);
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueBinding2));
    rmqamqp::TopologyMerger::merge(topology, topologyUpdate);
    EXPECT_EQ(1, topology.queueBindings.size());
    EXPECT_EQ("bindKey", topology.queueBindings[0]->bindingKey());
}

TEST_F(TopologyMergerTests, unbindExistingBind)
{
    rmqt::Topology topology;
    rmqt::TopologyUpdate topologyUpdate;

    rmqt::FieldTable args;
    bsl::shared_ptr<rmqt::QueueBinding> queueBinding =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, "bindKey", args);
    bsl::shared_ptr<rmqt::QueueBinding> queueBinding2 =
        bsl::make_shared<rmqt::QueueBinding>(exchange, queue, "bindKey2", args);
    topology.queueBindings.push_back(queueBinding2);

    bsl::shared_ptr<rmqt::QueueUnbinding> queueUnbinding =
        bsl::make_shared<rmqt::QueueUnbinding>(
            exchange, queue, "bindKey", args);
    topologyUpdate.updates.push_back(
        rmqt::TopologyUpdate::SupportedUpdate(queueUnbinding));

    rmqamqp::TopologyMerger::merge(topology, topologyUpdate);
    EXPECT_EQ(1, topology.queueBindings.size());
    EXPECT_EQ("bindKey2", topology.queueBindings[0]->bindingKey());
}
