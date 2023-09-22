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

#ifndef INCLUDED_RMQTESTMOCKS_MOCKRABBITCONTEXT
#define INCLUDED_RMQTESTMOCKS_MOCKRABBITCONTEXT

#include <rmqa_rabbitcontext.h>
#include <rmqp_rabbitcontext.h>

#include <rmqa_vhost.h>
#include <rmqp_connection.h>
#include <rmqp_rabbitcontext.h>
#include <rmqt_credentials.h>
#include <rmqt_endpoint.h>
#include <rmqt_future.h>
#include <rmqt_vhostinfo.h>

#include <gmock/gmock.h>

#include <bslma_managedptr.h>

#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqtestmocks {

/// \class MockRabbitContext
/// \brief Mocks rmqp::RabbitContext and/or rmqa::RabbitContext

class MockRabbitContext : public rmqp::RabbitContext {
  public:
    // CREATORS
    MockRabbitContext();
    ~MockRabbitContext() BSLS_KEYWORD_OVERRIDE;

    bsl::shared_ptr<rmqa::RabbitContext> context();

    MOCK_METHOD3(createVHostConnection,
                 bsl::shared_ptr<rmqp::Connection>(
                     const bsl::string& userDefinedName,
                     const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
                     const bsl::shared_ptr<rmqt::Credentials>& credentials));

    MOCK_METHOD2(
        createVHostConnection,
        bsl::shared_ptr<rmqp::Connection>(const bsl::string& userDefinedName,
                                          const rmqt::VHostInfo& vhostInfo));

  private:
    MockRabbitContext(const MockRabbitContext&) BSLS_KEYWORD_DELETED;
    MockRabbitContext& operator=(const MockRabbitContext&) BSLS_KEYWORD_DELETED;

}; // class RabbitContext

} // namespace rmqtestmocks
} // namespace BloombergLP

#endif // ! INCLUDED_RMQTESTMOCKS_MOCKRABBITCONTEXT
