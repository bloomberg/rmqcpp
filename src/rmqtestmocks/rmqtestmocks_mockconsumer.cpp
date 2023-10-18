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

#include <rmqtestmocks_mockconsumer.h>

#include <rmqt_future.h>
#include <rmqt_result.h>

namespace BloombergLP {
namespace rmqtestmocks {

MockConsumer::MockConsumer() {}
MockConsumer::~MockConsumer() {}

bsl::shared_ptr<rmqa::Consumer> MockConsumer::consumer()
{
    bslma::ManagedPtr<rmqp::Consumer> consumer(
        (rmqp::Consumer*)this,
        bslma::Default::allocator(),
        bslma::ManagedPtrUtil::noOpDeleter);
    return bsl::make_shared<rmqa::Consumer>(bsl::ref(consumer));
}

rmqt::Result<rmqp::Consumer> MockConsumer::success()
{
    return rmqt::Result<rmqp::Consumer>(
        bsl::shared_ptr<rmqp::Consumer>(this, bslstl::SharedPtrNilDeleter()));
}

rmqt::Result<rmqp::Consumer> MockConsumer::timeout()
{
    return rmqt::Result<rmqp::Consumer>("TIMED OUT", rmqt::TIMEOUT);
}

rmqt::Result<rmqp::Consumer> MockConsumer::error()
{
    return rmqt::Result<rmqp::Consumer>("Error Message", 1);
}

rmqt::Future<rmqp::Consumer> MockConsumer::successAsync()
{
    rmqt::Future<rmqp::Consumer>::Pair future =
        rmqt::Future<rmqp::Consumer>::make();
    future.first(success());

    return future.second;
}

rmqt::Future<rmqp::Consumer> MockConsumer::timeoutAsync()
{
    rmqt::Future<rmqp::Consumer>::Pair future =
        rmqt::Future<rmqp::Consumer>::make();

    future.first(timeout());

    return future.second;
}

rmqt::Future<rmqp::Consumer> MockConsumer::errorAsync()
{
    rmqt::Future<rmqp::Consumer>::Pair future =
        rmqt::Future<rmqp::Consumer>::make();
    future.first(error());

    return future.second;
}

} // namespace rmqtestmocks
} // namespace BloombergLP
