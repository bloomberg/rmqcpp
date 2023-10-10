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

#include <rmqa_connectionmonitor.h>

#include <rmqamqp_channelcontainer.h>
#include <rmqamqp_channelmap.h>

#include <ball_log.h>

#include <bsl_algorithm.h>
#include <bsl_list.h>
#include <bsl_map.h>
#include <bsl_memory.h>
#include <bsl_utility.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqa {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQA.CONNECTIONMONITOR")

void reportHungMessage(
    const rmqamqp::MessageStore<rmqt::Message>::Entry& message)
{
    const bdlt::DatetimeInterval age =
        bdlt::CurrentTime::utc() - message.second.second;

    BALL_LOG_WARN << "Hung message detected. Consumer message processing time "
                     "exceeds timeout specified in "
                     "RabbitContextOptions. Message: "
                  << message.second.first << ", delivery tag: " << message.first
                  << ", age: " << age.totalSecondsAsDouble() << " seconds";
}

bool connectionDestructed(
    const bsl::weak_ptr<rmqamqp::ChannelContainer>& connection)
{
    return !connection.lock();
}
} // namespace

ConnectionMonitor::ConnectionMonitor(
    const bsls::TimeInterval& messageProcessingTimeout,
    const HungMessageCallback& callback /* = HungMessageCallback() */)
: d_messageProcessingTimeout(messageProcessingTimeout)
{
    if (callback) {
        d_callback = callback;
    }
    else {
        d_callback = &reportHungMessage;
    }
}

void ConnectionMonitor::addConnection(
    const bsl::weak_ptr<rmqamqp::ChannelContainer>& connection)
{
    d_connections.push_back(connection);
}

void ConnectionMonitor::processHungMessages(
    const rmqamqp::MessageStore<rmqt::Message>::MessageList& hungMessages)
{
    bsl::for_each(hungMessages.begin(), hungMessages.end(), d_callback);
}

void ConnectionMonitor::run()
{
    bdlt::Datetime cutoffTime =
        bdlt::CurrentTime::utc() - d_messageProcessingTimeout;
    BALL_LOG_DEBUG << "Cleaning up expired connections";
    // Remove expired connections
    d_connections.remove_if(&connectionDestructed);
    for (bsl::list<bsl::weak_ptr<rmqamqp::ChannelContainer> >::iterator conn =
             d_connections.begin();
         conn != d_connections.end();
         ++conn) {
        bsl::shared_ptr<rmqamqp::ChannelContainer> connection = conn->lock();
        if (connection) {
            const rmqamqp::ChannelMap::ReceiveChannelMap& receiveChannelMap =
                connection->channelMap().getReceiveChannels();
            for (rmqamqp::ChannelMap::ReceiveChannelMap::const_iterator it =
                     receiveChannelMap.cbegin();
                 it != receiveChannelMap.cend();
                 ++it) {
                BALL_LOG_DEBUG
                    << "Checking for hung messages older than "
                    << d_messageProcessingTimeout.totalSecondsAsDouble()
                    << " seconds, for receive channel: " << it->first;
                processHungMessages(
                    it->second->getMessagesOlderThan(cutoffTime));
            }
        }
        else {
            BALL_LOG_ERROR << "Unexpected destructed connection";
        }
    }
}

bsl::shared_ptr<ConnectionMonitor::AliveConnectionInfo>
ConnectionMonitor::fetchAliveConnectionInfo()
{
    d_connections.remove_if(&connectionDestructed);

    bsl::shared_ptr<AliveConnectionInfo> result =
        bsl::make_shared<AliveConnectionInfo>();

    for (bsl::list<bsl::weak_ptr<rmqamqp::ChannelContainer> >::iterator conn =
             d_connections.begin();
         conn != d_connections.end();
         ++conn) {

        bsl::shared_ptr<rmqamqp::ChannelContainer> connection = conn->lock();

        if (!connection) {
            continue;
        }

        bsl::vector<bsl::string> aliveChannelInfo;

        const rmqamqp::ChannelMap::ReceiveChannelMap& receiveChannelMap =
            connection->channelMap().getReceiveChannels();

        for (rmqamqp::ChannelMap::ReceiveChannelMap::const_iterator it =
                 receiveChannelMap.cbegin();
             it != receiveChannelMap.cend();
             ++it) {
            aliveChannelInfo.push_back(it->second->channelDebugName());
        }

        const rmqamqp::ChannelMap::SendChannelMap& sendChannelMap =
            connection->channelMap().getSendChannels();

        for (rmqamqp::ChannelMap::SendChannelMap::const_iterator it =
                 sendChannelMap.cbegin();
             it != sendChannelMap.cend();
             ++it) {
            aliveChannelInfo.push_back(it->second->channelDebugName());
        }

        result->aliveConnectionChannelInfo.push_back(bsl::make_pair(
            connection->connectionDebugName(), aliveChannelInfo));
    }

    return result;
}

} // namespace rmqa
} // namespace BloombergLP
