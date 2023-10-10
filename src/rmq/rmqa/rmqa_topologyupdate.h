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

#ifndef INCLUDED_RMQA_TOPOLOGYUPDATE
#define INCLUDED_RMQA_TOPOLOGYUPDATE

#include <rmqa_topology.h>
#include <rmqp_topologyupdate.h>
#include <rmqt_properties.h>

#include <ball_log.h>

namespace BloombergLP {
namespace rmqa {
class TopologyUpdate : public rmqp::TopologyUpdate {
  public:
    BALL_LOG_SET_CLASS_CATEGORY("RMQA.TOPOLOGYUPDATE");

    TopologyUpdate();
    ~TopologyUpdate() BSLS_KEYWORD_OVERRIDE;

    /// Declare a binding between queue and exchange with the given bindingKey.
    ///
    /// This binding is declared to the broker, and if successful is applied to
    /// the topology declared on every reconnection
    void bind(const rmqt::ExchangeHandle& exchange,
              const rmqt::QueueHandle& queue,
              const bsl::string& bindingKey,
              const rmqt::FieldTable& args = rmqt::FieldTable())
        BSLS_KEYWORD_OVERRIDE;

    /// Declare an unbinding between queue and exchange with the given
    /// bindingKey.
    ///
    /// This unbinding is declared to the broker, and if successful is applied
    /// to the topology declared on every reconnection
    void unbind(const rmqt::ExchangeHandle& exchange,
                const rmqt::QueueHandle& queue,
                const bsl::string& bindingKey,
                const rmqt::FieldTable& args = rmqt::FieldTable())
        BSLS_KEYWORD_OVERRIDE;

#ifdef USES_LIBRMQ_EXPERIMENTAL_FEATURES
    /// \brief Delete a queue from the vhost.
    /// This method deletes a queue from the vhost. On confirmation from the
    /// broker the number of messages deleted along with the queue is logged at
    /// INFO level, but not the name of the queue.
    /// If the queue does not exist on the vhost, broker will confirm this
    /// operation as successful.
    /// \param name Name of the queue to delete.
    /// \param ifUnused If set to IF_UNUSED,
    ///        broker will delete the queue only
    ///        if the queue has no active consumers. If set to ALLOW_IN_USE,
    ///        broker will delete the queue regardless of any consumers.
    /// \param ifEmpty If set to IF_EMPTY, broker will delete the queue only if
    ///        there are no messages in it. If set to ALLOW_MSG_DELETE, broker
    ///        will delete the queue along with any messages in it.
    void deleteQueue(const bsl::string& queueName,
                     rmqt::QueueUnused::Value ifUnused,
                     rmqt::QueueEmpty::Value ifEmpty);
#endif

    /// \brief Topology containing only updates to the initial state
    virtual const rmqt::TopologyUpdate&
    topologyUpdate() const BSLS_KEYWORD_OVERRIDE;

  private:
    rmqt::TopologyUpdate d_topologyUpdate;
};

} // namespace rmqa
} // namespace BloombergLP
#endif // INCLUDED_RMQA_TOPOLOGYUPDATE
