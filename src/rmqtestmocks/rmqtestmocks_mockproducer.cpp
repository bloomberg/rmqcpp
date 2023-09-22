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

#include <rmqtestmocks_mockproducer.h>

#include <rmqt_future.h>
#include <rmqt_result.h>

namespace BloombergLP {
namespace rmqtestmocks {

MockProducer::MockProducer() {}
MockProducer::~MockProducer() {}

bsl::shared_ptr<rmqa::Producer> MockProducer::producer()
{
    bslma::ManagedPtr<rmqp::Producer> producer(
        (rmqp::Producer*)this,
        bslma::Default::allocator(),
        bslma::ManagedPtrUtil::noOpDeleter);
    return bsl::make_shared<rmqa::Producer>(bsl::ref(producer));
}

rmqt::Result<rmqp::Producer> MockProducer::success()
{
    return rmqt::Result<rmqp::Producer>(
        bsl::shared_ptr<rmqp::Producer>(this, bslstl::SharedPtrNilDeleter()));
}

rmqt::Result<rmqp::Producer> MockProducer::timeout()
{
    return rmqt::Result<rmqp::Producer>("TIMED OUT", rmqt::TIMEOUT);
}

rmqt::Result<rmqp::Producer> MockProducer::error()
{
    return rmqt::Result<rmqp::Producer>("Error Message", 1);
}

rmqt::Future<rmqp::Producer> MockProducer::successAsync()
{
    rmqt::Future<rmqp::Producer>::Pair future =
        rmqt::Future<rmqp::Producer>::make();
    future.first(success());

    return future.second;
}

rmqt::Future<rmqp::Producer> MockProducer::timeoutAsync()
{
    rmqt::Future<rmqp::Producer>::Pair future =
        rmqt::Future<rmqp::Producer>::make();

    future.first(timeout());

    return future.second;
}

rmqt::Future<rmqp::Producer> MockProducer::errorAsync()
{
    rmqt::Future<rmqp::Producer>::Pair future =
        rmqt::Future<rmqp::Producer>::make();
    future.first(error());

    return future.second;
}

} // namespace rmqtestmocks
} // namespace BloombergLP
