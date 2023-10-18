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

#ifndef INCLUDED_RMQA_CONNECTIONMONITOR
#define INCLUDED_RMQA_CONNECTIONMONITOR

#include <rmqamqp_channelcontainer.h>
#include <rmqamqp_connection.h>
#include <rmqamqp_connectionmonitor.h>
#include <rmqamqp_messagestore.h>
#include <rmqio_task.h>
#include <rmqt_future.h>
#include <rmqt_message.h>

#include <bsl_functional.h>
#include <bsls_timeinterval.h>

#include <bsl_list.h>
#include <bsl_memory.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqa {

class ConnectionMonitor : public rmqamqp::ConnectionMonitor,
                          public rmqio::Task {
  public:
    typedef bsl::function<void(
        const rmqamqp::MessageStore<rmqt::Message>::Entry&)>
        HungMessageCallback;

    explicit ConnectionMonitor(
        const bsls::TimeInterval& messageProcessingTimeout,
        const HungMessageCallback& callback = HungMessageCallback());

    void
    addConnection(const bsl::weak_ptr<rmqamqp::ChannelContainer>& connection)
        BSLS_KEYWORD_OVERRIDE;

    void run() BSLS_KEYWORD_OVERRIDE;

    struct AliveConnectionInfo {
        typedef bsl::pair<bsl::string, bsl::vector<bsl::string> >
            ConnectionChannelsInfo;

        bsl::vector<ConnectionChannelsInfo> aliveConnectionChannelInfo;
    };

    /// Fetch information about every connection which we have valid weak_ptr.
    /// This is useful for debugging leaked connection handles.
    bsl::shared_ptr<AliveConnectionInfo> fetchAliveConnectionInfo();

  private:
    void processHungMessages(
        const rmqamqp::MessageStore<rmqt::Message>::MessageList& hungMessages);
    bsls::TimeInterval d_messageProcessingTimeout;
    HungMessageCallback d_callback;
    bsl::list<bsl::weak_ptr<rmqamqp::ChannelContainer> > d_connections;
};

} // namespace rmqa
} // namespace BloombergLP

#endif
