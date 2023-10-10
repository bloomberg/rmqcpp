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

#ifndef INCLUDED_RMQP_RABBITCONTEXT
#define INCLUDED_RMQP_RABBITCONTEXT

#include <rmqp_connection.h>

#include <rmqt_credentials.h>
#include <rmqt_endpoint.h>
#include <rmqt_future.h>
#include <rmqt_vhostinfo.h>

#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqp {

/// \class RabbitContext
/// \brief Interface for spawning RabbitMQ connections
///
/// rmqp::RabbitContext allows creation of RabbitMQ connections. The
/// implementation, typically `rmqa::RabbitContext` will hold any resources
/// required for creating these See `rmqa::RabbitContext` documentation for
/// usage

class RabbitContext {
  public:
    RabbitContext();
    virtual ~RabbitContext();

    /// \brief Connect to a RabbitMQ broker
    ///
    /// \param endpoint identifies the broker to connect to
    /// \param credentials Authentication information
    ///
    /// Create a new rmqa::VHost to a RabbitMQ broker.
    /// Connections will be established (with the broker) when creating
    /// producers or consumers on the vhost object
    virtual bsl::shared_ptr<Connection> createVHostConnection(
        const bsl::string& userDefinedName,
        const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
        const bsl::shared_ptr<rmqt::Credentials>& credentials) = 0;

    /// \brief Connect to a RabbitMQ broker
    ///
    /// \param vhostInfo identifies the broker to connect to & authentication
    /// details
    ///
    /// Create a new rmqa::VHost to a RabbitMQ broker.
    /// Connections will be established (with the broker) when creating
    /// producers or consumers on the vhost object
    virtual bsl::shared_ptr<Connection>
    createVHostConnection(const bsl::string& userDefinedName,
                          const rmqt::VHostInfo& endpoint) = 0;

  private:
    RabbitContext(const RabbitContext&) BSLS_KEYWORD_DELETED;
    RabbitContext& operator=(const RabbitContext&) BSLS_KEYWORD_DELETED;

}; // class RabbitContext

} // namespace rmqp
} // namespace BloombergLP

#endif // ! INCLUDED_RMQP_RABBITCONTEXT
