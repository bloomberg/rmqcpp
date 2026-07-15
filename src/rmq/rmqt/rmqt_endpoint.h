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

#ifndef INCLUDED_RMQT_ENDPOINT
#define INCLUDED_RMQT_ENDPOINT

#include <bsl_cstdint.h>
#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

class SecurityParameters;

/// \brief Base class for AMQP endpoint

class Endpoint {
  public:
    virtual ~Endpoint() {};
    virtual bsl::string formatAddress() const = 0;
    virtual bsl::string hostname() const      = 0;
    virtual bsl::string vhost() const         = 0;
    virtual bsl::uint16_t port() const        = 0;
    virtual bsl::shared_ptr<rmqt::SecurityParameters>
    securityParameters() const;

    /// \brief Invoked each time an AMQP connection to this endpoint is
    /// established (Connection.Open-Ok received). Defaults to a no-op;
    /// implementations may override it to observe the connection lifecycle,
    /// e.g. for monitoring or metrics.
    ///
    /// \warning Invoked synchronously on the connection's event-loop thread
    /// (the same context as the connection's other callbacks). Overrides must
    /// be cheap and non-blocking and must not call back into the connection.
    /// A single `Endpoint` may be shared (via `shared_ptr`) by more than one
    /// connection, and those connections may run on different event-loop
    /// threads; an override must therefore synchronise any state it mutates.
    virtual void onConnectSuccess() {}

    /// \brief Invoked each time the connection to this endpoint is not
    /// established and will be retried -- either an establishment attempt
    /// failed, or a previously-established connection was lost and will
    /// reconnect. Defaults to a no-op; implementations may override it to
    /// observe the connection lifecycle, e.g. for monitoring or metrics.
    ///
    /// \warning Invoked synchronously on the connection's event-loop thread
    /// (the same context as the connection's other callbacks). Overrides must
    /// be cheap and non-blocking and must not call back into the connection.
    /// A single `Endpoint` may be shared (via `shared_ptr`) by more than one
    /// connection, and those connections may run on different event-loop
    /// threads; an override must therefore synchronise any state it mutates.
    ///
    /// \note Only invoked when the connection will be retried. It is not
    /// invoked on a terminal failure (e.g. when the connection error threshold
    /// is breached and the failure is surfaced to the application).
    virtual void onConnectFailed() {}
};

} // namespace rmqt
} // namespace BloombergLP

#endif
