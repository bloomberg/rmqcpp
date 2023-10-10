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

#ifndef INCLUDED_RMQP_TOPOLOGYUPDATE
#define INCLUDED_RMQP_TOPOLOGYUPDATE

#include <rmqt_fieldvalue.h>
#include <rmqt_topology.h>
#include <rmqt_topologyupdate.h>

#include <bsl_string.h>

namespace BloombergLP {
namespace rmqp {

/// \brief An interface providing a manipulatable RabbitMQ topology update
/// structure
///
/// Represents the RabbitMQ topology update, allowing declaring of Bindings and
/// Unbindings. This object is passed to either rmqa::Producer or rmqa::Consumer
/// updateTopology method
class TopologyUpdate {
  public:
    // CREATORS
    virtual ~TopologyUpdate();

    /// Declare a binding between queue and exchange with the given bindingKey.
    ///
    /// This binding is declared to the broker, and if successful is applied to
    /// the topology declared on every reconnection
    virtual void bind(const rmqt::ExchangeHandle& exchange,
                      const rmqt::QueueHandle& queue,
                      const bsl::string& bindingKey,
                      const rmqt::FieldTable& args = rmqt::FieldTable()) = 0;

    /// Declare an unbinding between queue and exchange with the given
    /// bindingKey.
    ///
    /// This unbinding is declared to the broker, and if successful is applied
    /// to the topology declared on every reconnection
    virtual void unbind(const rmqt::ExchangeHandle& exchange,
                        const rmqt::QueueHandle& queue,
                        const bsl::string& bindingKey,
                        const rmqt::FieldTable& args = rmqt::FieldTable()) = 0;

    /// \brief Get a readonly copy of stored topology update
    ///
    /// This is used internally by `rmqamqp` to send the topology update to the
    /// broker
    virtual const rmqt::TopologyUpdate& topologyUpdate() const = 0;
};

} // namespace rmqp
} // namespace BloombergLP

#endif // INCLUDED_RMQP_TOPOLOGYUPDATE
