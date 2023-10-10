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

#ifndef INCLUDED_RMQA_RABBITCONTEXT
#define INCLUDED_RMQA_RABBITCONTEXT

#include <rmqa_rabbitcontextoptions.h>
#include <rmqa_topology.h>
#include <rmqa_vhost.h>

#include <rmqp_metricpublisher.h>
#include <rmqp_rabbitcontext.h>
#include <rmqp_topology.h>

#include <rmqt_credentials.h>
#include <rmqt_endpoint.h>
#include <rmqt_future.h>
#include <rmqt_vhostinfo.h>

#include <bdlmt_threadpool.h>

#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqa {
class RabbitContextImpl;

/// \class RabbitContext
/// \brief Owns resources shared between multiple RabbitMQ connections
///
/// rmqa::RabbitContext Owns the threadpool & async event loop which is shared
/// between connections. rmqa::RabbitContext must live longer than all
/// connections created from it.

class RabbitContext {
  public:
    // CREATORS
    /// \brief Construct a RabbitContext
    ///
    /// \param options RabbitContextOptions object with user-specified
    /// arguments
    explicit RabbitContext(
        const RabbitContextOptions& options = RabbitContextOptions());

    /// \brief Construct a RabbitContext with a different implementation
    /// This is primarily used by mock objects for unit tests
    ///
    /// \param impl implements all methods RabbitContext provides
    explicit RabbitContext(bslma::ManagedPtr<rmqp::RabbitContext> impl);

    /// Trigger the destruction of the event loop, and (if not passed in) the
    /// threadpool. The destructor will block waiting for the eventloop and
    /// threadpool threads to `join()`. rmqa::VHost handles should be
    /// destructed before the RabbitContext to avoid waiting indefinitely.
    ~RabbitContext();

    /// \brief Connect to a RabbitMQ broker
    ///
    /// \param endpoint identifies the broker to connect to
    /// \param credentials Authentication information
    ///
    /// Create a new rmqa::VHost to a RabbitMQ broker.
    /// Connections will be established (with the broker) when creating
    /// producers or consumers on the vhost object
    bsl::shared_ptr<VHost> createVHostConnection(
        const bsl::string& userDefinedName,
        const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
        const bsl::shared_ptr<rmqt::Credentials>& credentials);

    /// \brief Connect to a RabbitMQ broker
    ///
    /// \param vhostInfo identifies the broker to connect to & authentication
    /// details
    ///
    /// Create a new rmqa::VHost to a RabbitMQ broker.
    /// Connections will be established (with the broker) when creating
    /// producers or consumers on the vhost object
    bsl::shared_ptr<VHost>
    createVHostConnection(const bsl::string& userDefinedName,
                          const rmqt::VHostInfo& vhostInfo);

  private:
    bslma::ManagedPtr<rmqp::RabbitContext> d_impl;

  private:
    RabbitContext(const RabbitContext&) BSLS_KEYWORD_DELETED;
    RabbitContext& operator=(const RabbitContext&) BSLS_KEYWORD_DELETED;

}; // class RabbitContext

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_RABBITCONTEXT
