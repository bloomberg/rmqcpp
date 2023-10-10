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

// rmqamqp_framer.h
#ifndef INCLUDED_RMQAMQP_FRAMER
#define INCLUDED_RMQAMQP_FRAMER

#include <rmqamqp_contentmaker.h>
#include <rmqamqp_message.h>
#include <rmqamqpt_frame.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdlib.h>
#include <bsl_memory.h>
#include <bsl_unordered_map.h>
#include <bsl_vector.h>

//@PURPOSE: Construct rmqamqpt::Frame objects from rmqamqp:: objects
//
//@CLASSES:
//  rmqamqp::Framer: Statefully construct rmqamqp::Message objects from one or
//      more rmqamqpt::Frame objects. Also construct rmqamqpt::Frame objects
//      from rmqamqpt::Messge objects. Decoding from Frames is Stateful because
//      Content Messages can be spread over multiple frames. These content
//      frames can be multiplexed with multiple channels

namespace BloombergLP {
namespace rmqamqp {

class Framer {
  public:
    enum ReturnCode {
        OK = 0,
        PARTIAL,
        CHANNEL_EXCEPTION,
        CONNECTION_EXCEPTION
    };

    Framer();

    /// Updates the maximum frame size used when encoding Content frames.
    void setMaxFrameSize(bsl::size_t maxSize);

    /// Statefully constructs rmqamqp::Message objects from incoming frames
    /// Incoming Messages spread across frames are de-multiplexed by channel
    /// Partial message receipt is indicated by the ReturnCode PARTIAL.
    /// ReturnCode OK means that receiveMessage has been populated
    ReturnCode appendFrame(uint16_t* receiveChannel,
                           rmqamqp::Message* receiveMessage,
                           const rmqamqpt::Frame& frame);

    /// Clear any buffered frames for the given channel
    /// To be called when a channel is closed, to free up memory and allow
    /// channel number reuse
    void clearChannel(uint16_t channel);

    /// Constructs rmamqpt::Frames from a Message. Messages can be serialized
    /// into multiple frames (e.g. Content messages)
    void makeFrames(bsl::vector<rmqamqpt::Frame>* frames,
                    uint16_t channel,
                    const rmqamqp::Message& message) const;

    /// Constructs an rmqamqpt::Frame from an rmqamqpt::Method
    /// This is a specialisation of `makeFrames` as Method messages are
    /// always framed into one rmqamqpt::Frame
    static void makeMethodFrame(rmqamqpt::Frame* frame,
                                uint16_t channel,
                                const rmqamqpt::Method& method);

    /// Constructs an rmqamqpt::Frame for a Heartbeat message
    static rmqamqpt::Frame makeHeartbeatFrame();

    /// Constructs a content header frame for a message
    static rmqamqpt::Frame makeContentHeaderFrame(const rmqt::Message& message,
                                                  uint16_t channel);

    /// Constructs a content body frame for a message
    static rmqamqpt::Frame makeContentBodyFrame(const uint8_t* message,
                                                const size_t encodedFrameSize,
                                                const size_t encodedPayloadSize,
                                                uint16_t channel);

    static void encodeFrameHeader(rmqamqpt::Writer& output,
                                  uint8_t type,
                                  uint16_t channel,
                                  size_t payloadLength);

    static void encodeFrameEnd(rmqamqpt::Writer& output);

  private:
    typedef bsl::unordered_map<uint16_t, bsl::shared_ptr<ContentMaker> >
        ChannelContentMaker;

    ChannelContentMaker d_channelContentMakers;
    size_t d_maxFrameSize;
}; // class Framer

} // namespace rmqamqp
} // namespace BloombergLP

#endif // ! INCLUDED_RMQAMQP_FRAMER
