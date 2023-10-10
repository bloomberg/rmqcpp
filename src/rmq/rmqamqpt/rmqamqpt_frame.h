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

#ifndef INCLUDED_RMQAMQPT_FRAME
#define INCLUDED_RMQAMQPT_FRAME

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

class Frame {
  public:
    enum ReturnCode {
        OK = 0,      ///< Frame decoded
        PARTIAL,     ///< Could not decode an entire frame
        DECODE_ERROR ///< Corrupt/invalid data received
    };

    Frame();
    Frame(uint8_t type,
          uint16_t channel,
          const uint8_t* r,
          uint32_t totalDataSize);
    Frame(uint8_t type,
          uint16_t channel,
          const bsl::shared_ptr<bsl::vector<uint8_t> >& serializedData);

    const uint8_t* payload() const;
    bsl::size_t payloadLength() const;

    const uint8_t* rawData() const;
    const bsl::shared_ptr<bsl::vector<uint8_t> > serializedData() const
    {
        return d_serializedData;
    }
    uint8_t type() const;
    uint16_t channel() const;

    /// Attempt to decode buffer into frame.
    /// \return OK if a whole frame is decoded
    /// \return PARTIAL if more data is needed to decode a frame
    /// \return DECODE_ERROR if no frame can be read from the stream. The
    ///                      stream will never be useful after this error is
    ///                      returned
    static ReturnCode decode(Frame* frame,
                             bsl::size_t* readBytes,
                             bsl::size_t* missingBytes,
                             const uint8_t* buffer,
                             bsl::size_t bufferLen);

    /// Return the maximum frame size we support
    static bsl::size_t getMaxFrameSize();

    /// Return the overhead of the framing, beyond the payload size
    static bsl::size_t frameOverhead();

    /// Return the size of the frame header
    static bsl::size_t frameHeaderSize();

    /// Return total frame size
    bsl::size_t totalFrameSize() const;

    /// Return calculated frame size before frame encoding
    static size_t calculateFrameSize(size_t payloadSize);

    bool operator==(const Frame& other) const;
    bool operator!=(const Frame& other) const;

  private:
    uint8_t d_type;
    uint16_t d_channel;
    // TODO: bsl::vector may not be appropriate for multi-payload messages
    // Re-consider when designing how to present the payload to the user via
    // rmqa::{Async,Sync}Consumer
    bsl::shared_ptr<bsl::vector<uint8_t> > d_serializedData;

    static const bsl::size_t MAX_FRAME_SIZE;
};

inline bsl::size_t Frame::getMaxFrameSize() { return MAX_FRAME_SIZE; }

inline bsl::size_t Frame::frameOverhead()
{
    return frameHeaderSize() + sizeof(uint8_t); // sentinel value(frame end)
}

inline bsl::size_t Frame::frameHeaderSize()
{
    return sizeof(uint8_t) + sizeof(uint16_t) +
           sizeof(uint32_t); // sizeof(type) + sizeof(channel) +
                             // sizeof(payloadLength)
}

inline bsl::size_t Frame::totalFrameSize() const
{
    return d_serializedData ? d_serializedData->size() : 0;
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
