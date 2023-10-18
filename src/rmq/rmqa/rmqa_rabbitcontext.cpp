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

#include <rmqa_rabbitcontext.h>

#include <rmqa_rabbitcontextimpl.h>
#include <rmqa_vhost.h>
#include <rmqa_vhostimpl.h>

#include <rmqio_asioeventloop.h>
#include <rmqt_future.h>
#include <rmqt_message.h>
#include <rmqt_vhostinfo.h>

#include <bdlf_bind.h>
#include <bdlf_placeholder.h>
#include <bslma_managedptr.h>

#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqa {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.RABBITCONTEXT")
} // namespace

RabbitContext::RabbitContext(const RabbitContextOptions& options)
: d_impl(bslma::ManagedPtr<RabbitContextImpl>(new RabbitContextImpl(
      bslma::ManagedPtr<rmqio::EventLoop>(new rmqio::AsioEventLoop()),
      options)))
{
}

RabbitContext::RabbitContext(bslma::ManagedPtr<rmqp::RabbitContext> impl)
: d_impl(impl)
{
}

RabbitContext::~RabbitContext() {}

bsl::shared_ptr<rmqa::VHost> RabbitContext::createVHostConnection(
    const bsl::string& userDefinedName,
    const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqt::Credentials>& credentials)
{
    return bsl::shared_ptr<VHost>(new VHost(
        d_impl->createVHostConnection(userDefinedName, endpoint, credentials)
            .managedPtr()));
}

bsl::shared_ptr<rmqa::VHost>
RabbitContext::createVHostConnection(const bsl::string& userDefinedName,
                                     const rmqt::VHostInfo& vhostInfo)
{
    return bsl::shared_ptr<VHost>(
        new VHost(d_impl->createVHostConnection(userDefinedName, vhostInfo)
                      .managedPtr()));
}

} // namespace rmqa
} // namespace BloombergLP
