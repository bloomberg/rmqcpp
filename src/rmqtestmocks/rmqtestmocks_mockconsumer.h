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

#ifndef INCLUDED_RMQTESTMOCKS_MOCKCONSUMER
#define INCLUDED_RMQTESTMOCKS_MOCKCONSUMER

#include <rmqa_consumer.h>
#include <rmqp_consumer.h>
#include <rmqt_future.h>
#include <rmqt_message.h>

#include <gmock/gmock.h>

#include <bsl_memory.h>
#include <bsls_timeinterval.h>

namespace BloombergLP {
namespace rmqtestmocks {

/// \class MockConsumer
/// \brief Mocks rmqp::Consumer and/or rmqa::Consumer

class MockConsumer : public rmqp::Consumer {
  public:
    // CREATORS
    MockConsumer();
    ~MockConsumer() BSLS_KEYWORD_OVERRIDE;

    bsl::shared_ptr<rmqa::Consumer> consumer();

    rmqt::Result<rmqp::Consumer> success();
    static rmqt::Result<rmqp::Consumer> timeout();
    static rmqt::Result<rmqp::Consumer> error();

    rmqt::Future<rmqp::Consumer> successAsync();
    static rmqt::Future<rmqp::Consumer> timeoutAsync();
    static rmqt::Future<rmqp::Consumer> errorAsync();

    MOCK_METHOD0(cancel, rmqt::Future<>());
    MOCK_METHOD0(drain, rmqt::Future<>());
    MOCK_METHOD1(cancelAndDrain, rmqt::Result<>(const bsls::TimeInterval&));
    MOCK_METHOD2(updateTopology,
                 rmqt::Result<>(const rmqt::TopologyUpdate&,
                                const bsls::TimeInterval&));
    MOCK_METHOD1(updateTopologyAsync,
                 rmqt::Future<>(const rmqt::TopologyUpdate&));

  private:
    MockConsumer(const MockConsumer&) BSLS_KEYWORD_DELETED;
    MockConsumer& operator=(const MockConsumer&) BSLS_KEYWORD_DELETED;

}; // class MockConsumer

} // namespace rmqtestmocks
} // namespace BloombergLP

#endif // ! INCLUDED_RMQTESTMOCKS_MOCKCONSUMER
