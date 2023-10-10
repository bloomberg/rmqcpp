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

#include <rmqio_decoder.h>

#include <rmqamqpt_frame.h>

#include <bslma_managedptr.h>

#include <bsl_cstdint.h>
#include <bsl_cstdio.h>
#include <bsl_stdexcept.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqio {

bslma::ManagedPtr<Decoder> Decoder::create(bsl::size_t maxFrameSize)
{
    return bslma::ManagedPtrUtil::makeManaged<Decoder>(maxFrameSize);
}

Decoder::Decoder(bsl::size_t maxFrameSize)
: d_maxFrameSize(maxFrameSize)
, d_buffer()
{
}

Decoder::ReturnCode
Decoder::appendBytes(bsl::vector<rmqamqpt::Frame>* outFrames,
                     const void* buffer,
                     bsl::size_t bufferLength)
{
    const bsl::uint8_t* data = static_cast<const bsl::uint8_t*>(buffer);
    d_buffer.insert(d_buffer.end(), data, &data[bufferLength]);

    bsl::size_t totalRead = 0;

    const bsl::uint8_t* readFrom = &d_buffer[0];
    bsl::size_t dataLength       = d_buffer.size();

    bsl::size_t readBytes = 0;
    rmqamqpt::Frame frame;
    bsl::size_t missing = 0;

    rmqamqpt::Frame::ReturnCode rc;

    while (rmqamqpt::Frame::OK ==
           (rc = rmqamqpt::Frame::decode(
                &frame, &readBytes, &missing, readFrom, dataLength))) {
        outFrames->push_back(frame);

        readFrom += readBytes;
        dataLength -= readBytes;

        totalRead += readBytes;
    }

    if (rc == rmqamqpt::Frame::DECODE_ERROR) {
        return Decoder::DECODE_ERROR;
    }

    if (totalRead > 0) {
        // Remove consumed data from d_buffer
        d_buffer.erase(d_buffer.begin(), d_buffer.begin() + totalRead);
    }

    // Check the buffered un-frameable data is below the d_maxFrameSize
    // threshold
    if (d_buffer.size() > d_maxFrameSize) {
        return Decoder::MAX_FRAME_SIZE;
    }

    return Decoder::OK;
}

} // namespace rmqio
} // namespace BloombergLP
