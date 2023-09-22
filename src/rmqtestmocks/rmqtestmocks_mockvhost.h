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

#ifndef INCLUDED_RMQTESTMOCKS_MOCKVHOST
#define INCLUDED_RMQTESTMOCKS_MOCKVHOST

#include <rmqa_vhost.h>
#include <rmqp_consumer.h>
#include <rmqp_producer.h>
#include <rmqt_future.h>
#include <rmqt_message.h>

#include <gmock/gmock.h>

#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqtestmocks {

/// \class MockVHost
/// \brief Mocks an rmqa::VHost object

class MockVHost : public rmqp::Connection {
  public:
    // CREATORS
    MockVHost();
    ~MockVHost() BSLS_KEYWORD_OVERRIDE;

    bsl::shared_ptr<rmqp::Connection> vhost();

    MOCK_METHOD0(close, void());

    MOCK_METHOD4(createConsumer,
                 rmqt::Result<rmqp::Consumer>(
                     const rmqt::Topology& topology,
                     rmqt::QueueHandle queue,
                     const rmqp::Consumer::ConsumerFunc& messageConsumer,
                     const rmqt::ConsumerConfig& config));

    MOCK_METHOD4(createConsumerAsync,
                 rmqt::Future<rmqp::Consumer>(
                     const rmqt::Topology& topology,
                     rmqt::QueueHandle queue,
                     const rmqp::Consumer::ConsumerFunc& messageConsumer,
                     const rmqt::ConsumerConfig& config));

    MOCK_METHOD3(createProducer,
                 rmqt::Result<rmqp::Producer>(const rmqt::Topology& topology,
                                              rmqt::ExchangeHandle exchange,
                                              uint16_t maxOutstandingConfirms));

    MOCK_METHOD3(createProducerAsync,
                 rmqt::Future<rmqp::Producer>(const rmqt::Topology& topology,
                                              rmqt::ExchangeHandle exchange,
                                              uint16_t maxOutstandingConfirms));
    // DEPRECATED:

    MOCK_METHOD5(createConsumer,
                 rmqt::Result<rmqp::Consumer>(
                     const rmqt::Topology& topology,
                     rmqt::QueueHandle queue,
                     const rmqp::Consumer::ConsumerFunc& messageConsumer,
                     const bsl::string& consumerTag,
                     uint16_t prefetchCount));

    MOCK_METHOD5(createConsumerAsync,
                 rmqt::Future<rmqp::Consumer>(
                     const rmqt::Topology& topology,
                     rmqt::QueueHandle queue,
                     const rmqp::Consumer::ConsumerFunc& messageConsumer,
                     const bsl::string& consumerTag,
                     uint16_t prefetchCount));

  private:
    MockVHost(const MockVHost&) BSLS_KEYWORD_DELETED;
    MockVHost& operator=(const MockVHost&) BSLS_KEYWORD_DELETED;

}; // class MockVhost

} // namespace rmqtestmocks
} // namespace BloombergLP

#endif // ! INCLUDED_RMQTESTMOCKS_MOCKVHOST
