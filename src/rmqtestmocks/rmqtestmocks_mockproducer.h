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

#ifndef INCLUDED_RMQTESTMOCKS_MOCKPRODUCER
#define INCLUDED_RMQTESTMOCKS_MOCKPRODUCER

#include <rmqa_producer.h>
#include <rmqp_producer.h>
#include <rmqt_future.h>
#include <rmqt_message.h>

#include <gmock/gmock.h>

#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqtestmocks {

/// \class MockProducer
/// \brief Mocks rmqp::Producer and/or rmqa::Producer

class MockProducer : public rmqp::Producer {
  public:
    // CREATORS
    MockProducer();
    ~MockProducer() BSLS_KEYWORD_OVERRIDE;

    bsl::shared_ptr<rmqa::Producer> producer();

    rmqt::Result<rmqp::Producer> success();
    static rmqt::Result<rmqp::Producer> timeout();
    static rmqt::Result<rmqp::Producer> error();

    rmqt::Future<rmqp::Producer> successAsync();
    static rmqt::Future<rmqp::Producer> timeoutAsync();
    static rmqt::Future<rmqp::Producer> errorAsync();

    MOCK_METHOD4(
        send,
        rmqp::Producer::SendStatus(
            const rmqt::Message& message,
            const bsl::string& routingKey,
            const rmqp::Producer::ConfirmationCallback& confirmCallback,
            const bsls::TimeInterval& timeout));

    MOCK_METHOD5(
        send,
        rmqp::Producer::SendStatus(
            const rmqt::Message& message,
            const bsl::string& routingKey,
            rmqt::Mandatory::Value mandatory,
            const rmqp::Producer::ConfirmationCallback& confirmCallback,
            const bsls::TimeInterval& timeout));

    MOCK_METHOD3(
        trySend,
        rmqp::Producer::SendStatus(
            const rmqt::Message& message,
            const bsl::string& routingKey,
            const rmqp::Producer::ConfirmationCallback& confirmCallback));

    MOCK_METHOD1(waitForConfirms, rmqt::Result<>(const bsls::TimeInterval&));

    MOCK_METHOD2(updateTopology,
                 rmqt::Result<>(const rmqt::TopologyUpdate&,
                                const bsls::TimeInterval&));
    MOCK_METHOD1(updateTopologyAsync,
                 rmqt::Future<>(const rmqt::TopologyUpdate&));

  private:
    MockProducer(const MockProducer&) BSLS_KEYWORD_DELETED;
    MockProducer& operator=(const MockProducer&) BSLS_KEYWORD_DELETED;

}; // class MockProducer

} // namespace rmqtestmocks
} // namespace BloombergLP

#endif // ! INCLUDED_RMQTESTMOCKS_MOCKPRODUCER
