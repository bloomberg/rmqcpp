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

// rmqio_decoder.h
#ifndef INCLUDED_RMQIO_DECODER
#define INCLUDED_RMQIO_DECODER

#include <rmqamqpt_frame.h>

#include <bsls_keyword.h>

#include <bsl_cstdlib.h>
#include <bsl_vector.h>
#include <bslma_managedptr.h>

//@PURPOSE: Construct whole rmqamqpt::Frame objects from an incoming bytestream
//
//@CLASSES:
//  rmqio::Decoder: Constructs rmamqpt::Frames from an incoming byte stream,
//    buffering data until whole frame(s) can be constructed.
//    The decoder owns buffered data which cannot yet be used to construct
//    entire frames. Once a frame has been created, it takes ownership of all of
//    it's memory

namespace BloombergLP {
namespace rmqio {

class Decoder {
  public:
    enum ReturnCode {
        OK = 0,         ///< Data either decoded, or added to internal buffer
        MAX_FRAME_SIZE, ///< Could not decode an entire frame within
                        ///<     `d_maxFrameSize` limits
        DECODE_ERROR    ///< Corrupt/invalid data received
    };

    static bslma::ManagedPtr<Decoder> create(bsl::size_t maxFrameSize = 2048);

    /// Constructor
    /// \param maxFrameSize sets the limit after which `appendBytes` will
    /// return
    ///     an error code if a frame cannot be constructed. Indicating either
    ///     invalid data or a large, unsupported, frame size.
    explicit Decoder(bsl::size_t maxFrameSize);

    /// Add `buffer` bytes to the internal buffer, and attempt to decode whole
    /// Frame(s) using the new, larger, internal buffer.
    /// \param outFrames Container which have been decoded using the new data
    /// are
    ///      placed into
    /// \param buffer A byte-array containing potentially-partial frame data
    /// \param bufferLength The safe number of bytes which can be read from
    ///      `buffer`
    ///
    /// \return ReturnCode indicating the status/health of the Decoder after
    ///         receiving data. See `ReturnCode` for details
    virtual ReturnCode appendBytes(bsl::vector<rmqamqpt::Frame>* outFrames,
                                   const void* buffer,
                                   bsl::size_t bufferLength);

    virtual ~Decoder() {}

    bsl::size_t maxFrameSize() const { return d_maxFrameSize; }

  private:
    Decoder(const Decoder&) BSLS_KEYWORD_DELETED;
    Decoder& operator=(const Decoder&) BSLS_KEYWORD_DELETED;

    bsl::size_t d_maxFrameSize;
    bsl::vector<uint8_t> d_buffer;

}; // class Decoder

} // namespace rmqio
} // namespace BloombergLP

#endif // ! INCLUDED_RMQIO_DECODER
