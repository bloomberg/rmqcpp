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

#ifndef INCLUDED_RMQAMQP_CHANNELMAP
#define INCLUDED_RMQAMQP_CHANNELMAP

#include <rmqamqp_channel.h>
#include <rmqamqp_message.h>
#include <rmqamqp_receivechannel.h>
#include <rmqamqp_sendchannel.h>

#include <bsl_cstdint.h>
#include <bsl_map.h>
#include <bsl_memory.h>

//@PURPOSE: Provide a container of Channels with associated Channel IDs
//
//@CLASSES: ChannelMap

namespace BloombergLP {
namespace rmqamqp {

class ChannelMap {
  public:
    typedef bsl::map<uint16_t, bsl::shared_ptr<Channel> > ChannelPtrMap;
    typedef bsl::map<uint16_t, bsl::shared_ptr<SendChannel> > SendChannelMap;
    typedef bsl::map<uint16_t, bsl::shared_ptr<ReceiveChannel> >
        ReceiveChannelMap;

    ChannelMap();

    /// Internally marks the next available channel id as occupied and returns
    /// its value
    uint16_t assignId();

    /// Associates the given channel with the passed channel id. Typically used
    /// after calling assignId This will overwrite any existing channel
    /// association
    void associateChannel(uint16_t channelId,
                          const bsl::shared_ptr<SendChannel>& channel);

    /// Associates the given channel with the passed channel id. Typically used
    /// after calling assignId This will overwrite any existing channel
    /// association
    void associateChannel(uint16_t channelId,
                          const bsl::shared_ptr<ReceiveChannel>& channel);

    /// Removes `channelId` from the container
    void removeChannel(uint16_t channelId);

    /// Call `reset` on every channel stored in the container
    void resetAll();

    /// Call `open` on every channel stored in the container
    void openAll();

    /// Call `processReceived` on the channel associated with `channelId`
    /// Returns false if there is no channel associated with that id
    bool processReceived(uint16_t channelId, const rmqamqp::Message& message);

    /// Returns send channels
    const SendChannelMap& getSendChannels() const { return d_sendChannels; }

    /// Returns receive channels
    const ReceiveChannelMap& getReceiveChannels() const
    {
        return d_receiveChannels;
    }

  private:
    ChannelPtrMap d_channels;
    SendChannelMap d_sendChannels;
    ReceiveChannelMap d_receiveChannels;
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
