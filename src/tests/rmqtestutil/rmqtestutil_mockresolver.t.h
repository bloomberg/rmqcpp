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

#ifndef INCLUDED_RMQTESTUTIL_MOCKRESOLVER_T
#define INCLUDED_RMQTESTUTIL_MOCKRESOLVER_T

#include <rmqio_connection.h>
#include <rmqio_resolver.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdint.h>
#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqtestutil {

class MockResolver : public rmqio::Resolver {
  public:
    MockResolver();
    virtual ~MockResolver();

    MOCK_METHOD6(
        asyncConnect,
        bsl::shared_ptr<rmqio::Connection>(const bsl::string&,
                                           bsl::uint16_t,
                                           size_t,
                                           const rmqio::Connection::Callbacks&,
                                           const NewConnectionCallback&,
                                           const ErrorCallback&));
    MOCK_METHOD7(asyncSecureConnect,
                 bsl::shared_ptr<rmqio::Connection>(
                     const bsl::string&,
                     bsl::uint16_t,
                     size_t,
                     const bsl::shared_ptr<rmqt::SecurityParameters>&,
                     const rmqio::Connection::Callbacks&,
                     const NewConnectionCallback&,
                     const ErrorCallback&));
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif
