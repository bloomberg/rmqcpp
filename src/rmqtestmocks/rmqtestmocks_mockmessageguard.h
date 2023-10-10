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

#ifndef INCLUDED_RMQTESTMOCKS_MOCKMESSAGEGUARD
#define INCLUDED_RMQTESTMOCKS_MOCKMESSAGEGUARD

#include <rmqp_messageguard.h>
#include <rmqt_consumerack.h>
#include <rmqt_message.h>

#include <gmock/gmock.h>

namespace BloombergLP {
namespace rmqtestmocks {

class MockMessageGuard : public rmqp::MessageGuard {
  public:
    MockMessageGuard();
    virtual ~MockMessageGuard();

    MOCK_CONST_METHOD0(message, const rmqt::Message&());
    MOCK_CONST_METHOD0(envelope, const rmqt::Envelope&());
    MOCK_METHOD0(ack, void());
    MOCK_METHOD1(nack, void(bool));
    MOCK_CONST_METHOD0(consumer, rmqp::Consumer*());

    MOCK_METHOD0(transferOwnership, rmqp::TransferrableMessageGuard());

  private:
    MockMessageGuard(const MockMessageGuard&);
    MockMessageGuard& operator=(const MockMessageGuard& other);

}; // class MockMessageGuard

} // namespace rmqtestmocks
} // namespace BloombergLP

#endif // ! INCLUDED_RMQTESTMOCKS_MOCKMESSAGEGUARD
