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

#include <rmqamqp_channelmap.h>

#include <ball_log.h>

#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.CHANNELMAP")
}

ChannelMap::ChannelMap()
: d_channels()
, d_sendChannels()
, d_receiveChannels()
{
}

uint16_t ChannelMap::assignId()
{
    uint16_t lowest = 1;

    for (ChannelPtrMap::const_iterator it = d_channels.begin();
         it != d_channels.end();
         ++it) {
        if (lowest < it->first) {
            break;
        }

        lowest = it->first + 1;
    }

    d_channels[lowest] = bsl::shared_ptr<Channel>();
    return lowest;
}

void ChannelMap::associateChannel(uint16_t channelId,
                                  const bsl::shared_ptr<SendChannel>& channel)
{
    d_sendChannels[channelId] = channel;
    d_channels[channelId]     = channel;
}

void ChannelMap::associateChannel(
    uint16_t channelId,
    const bsl::shared_ptr<ReceiveChannel>& channel)
{
    d_receiveChannels[channelId] = channel;
    d_channels[channelId]        = channel;
}

void ChannelMap::removeChannel(uint16_t channelId)
{
    d_channels.erase(channelId);
    d_sendChannels.erase(channelId);
    d_receiveChannels.erase(channelId);
    BALL_LOG_DEBUG << "Cleaned up channel " << channelId;
}

void ChannelMap::resetAll()
{
    bsl::vector<uint16_t> cleanupIds;

    for (ChannelPtrMap::iterator it = d_channels.begin();
         it != d_channels.end();
         ++it) {

        if (!it->second) {
            continue;
        }

        BALL_LOG_INFO << "Closing channel " << it->first;
        if (it->second->reset() == Channel::CLEANUP) {
            cleanupIds.push_back(it->first);
        }
    }

    for (bsl::vector<uint16_t>::const_iterator it = cleanupIds.begin();
         it != cleanupIds.end();
         ++it) {
        removeChannel(*it);
    }
}

void ChannelMap::openAll()
{
    for (ChannelPtrMap::iterator it = d_channels.begin();
         it != d_channels.end();
         ++it) {

        if (!it->second) {
            continue;
        }

        BALL_LOG_INFO << "Opening channel " << it->first;
        it->second->open();
    }
}

bool ChannelMap::processReceived(uint16_t channelId,
                                 const rmqamqp::Message& message)
{
    ChannelPtrMap::const_iterator it = d_channels.find(channelId);

    if (it == d_channels.end() || !it->second) {
        return false;
    }

    Channel::CleanupIndicator cleanupInd = it->second->processReceived(message);

    if (cleanupInd == Channel::CLEANUP) {
        // invalidates iterator
        removeChannel(channelId);
    }

    return true;
}

} // namespace rmqamqp
} // namespace BloombergLP
