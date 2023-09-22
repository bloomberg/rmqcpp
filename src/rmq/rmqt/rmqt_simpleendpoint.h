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

#ifndef INCLUDED_RMQT_SIMPLEENDPOINT
#define INCLUDED_RMQT_SIMPLEENDPOINT

#include <rmqt_endpoint.h>

#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief AMQP simple endpoint
///
/// This class provides AMQP simple endpoint. A simple endpoint consists of an
/// URI, a `vhost` name and a port number.

class SimpleEndpoint : public Endpoint {
  public:
    /// \brief SimpleEndpoint constructor
    /// \param address RabbitMQ broker URI
    /// \param vhost vhost name
    /// \param port port number
    SimpleEndpoint(bsl::string_view address,
                   bsl::string_view vhost,
                   bsl::uint16_t port = 5672);

    bsl::string formatAddress() const;
    bsl::string hostname() const;
    bsl::string vhost() const;
    unsigned short port() const;

  protected:
    virtual bsl::string protocol() const;

  private:
    bsl::string d_address;
    bsl::string d_vhost;
    bsl::uint16_t d_port;
};

} // namespace rmqt
} // namespace BloombergLP

#endif
