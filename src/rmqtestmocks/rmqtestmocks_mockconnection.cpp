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

namespace BloombergLP {
namespace rmqtestmocks {
MockConnection::MockConnection() {}
MockConnection::~MockConnection() {}

rmqt::Future<rmqp::Connection> MockConnection::success()
{
    rmqt::Future<rmqp::Connection>::Pair future =
        rmqt::Future<rmqp::Connection>::make();
    future.first(
        rmqt::Result<rmqp::Connection>(bsl::shared_ptr<rmqp::Connection>(
            this, bslstl::SharedPtrNilDeleter())));

    return future.second;
}

rmqt::Future<rmqp::Connection> MockConnection::timeout()
{
    rmqt::Future<rmqp::Connection>::Pair future =
        rmqt::Future<rmqp::Connection>::make();

    return future.second;
}

rmqt::Future<rmqp::Connection> MockConnection::error()
{
    rmqt::Future<rmqp::Connection>::Pair future =
        rmqt::Future<rmqp::Connection>::make();
    future.first(rmqt::Result<rmqp::Connection>("Error Message", 1));

    return future.second;
}

} // namespace rmqtestmocks
} // namespace BloombergLP
