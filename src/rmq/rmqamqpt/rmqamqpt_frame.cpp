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

#include <rmqamqpt_constants.h>
#include <rmqamqpt_frame.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>
#include <bsl_cstring.h>
#include <bsl_iostream.h>
#include <bsl_stdexcept.h>

namespace BloombergLP {
namespace rmqamqpt {

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.FRAME")

const bsl::size_t Frame::MAX_FRAME_SIZE = 150000;

Frame::Frame()
: d_type()
, d_channel()
, d_serializedData()
{
}

Frame::Frame(bsl::uint8_t type,
             uint16_t channel,
             const uint8_t* data,
             uint32_t totalDataSize)
: d_type(type)
, d_channel(channel)
, d_serializedData(
      bsl::make_shared<bsl::vector<uint8_t> >(data, data + totalDataSize))
{
}

Frame::Frame(uint8_t type,
             uint16_t channel,
             const bsl::shared_ptr<bsl::vector<uint8_t> >& serializedData)
: d_type(type)
, d_channel(channel)
, d_serializedData(serializedData)
{
}

Frame::ReturnCode Frame::decode(Frame* frame,
                                bsl::size_t* readBytes,
                                bsl::size_t* missingBytes,
                                const uint8_t* buffer,
                                bsl::size_t bufferLen)
{
    if (bufferLen < frameOverhead()) {
        return Frame::PARTIAL;
    }

    bsl::memcpy(&frame->d_type, &buffer[0], sizeof(frame->d_type));

    bdlb::BigEndianUint16 channel;
    bsl::memcpy(&channel, &buffer[1], sizeof(channel));
    frame->d_channel = channel;

    bdlb::BigEndianUint32 payloadLength;
    bsl::memcpy(&payloadLength, &buffer[3], sizeof(payloadLength));

    if ((frameOverhead() + payloadLength) > bufferLen) {
        return Frame::PARTIAL;
    }

    bsl::size_t buffer_len = frameHeaderSize() + payloadLength;
    if (buffer[buffer_len] != Constants::FRAME_END) {
        BALL_LOG_ERROR << "Decode error: expected Frame End marker. Frame: "
                       << static_cast<int>(frame->d_type) << " "
                       << frame->d_channel << " " << payloadLength;

        return Frame::DECODE_ERROR;
    }

    *missingBytes = bufferLen - (frameOverhead() + payloadLength);
    *readBytes    = payloadLength + frameOverhead();

    frame->d_serializedData = bsl::make_shared<bsl::vector<uint8_t> >(
        buffer, buffer + frameOverhead() + payloadLength);

    return Frame::OK;
}

bsl::size_t Frame::payloadLength() const
{
    return d_serializedData ? (d_serializedData->size() - frameOverhead()) : 0;
}

const uint8_t* Frame::rawData() const
{
    if (!d_serializedData || d_serializedData->empty()) {
        return NULL;
    }

    return d_serializedData->data();
}

const uint8_t* Frame::payload() const
{
    return rawData() ? (rawData() + frameHeaderSize()) : NULL;
}

size_t Frame::calculateFrameSize(size_t payloadSize)
{
    return payloadSize + frameOverhead();
}

uint8_t Frame::type() const { return d_type; }

uint16_t Frame::channel() const { return d_channel; }

bool Frame::operator==(const Frame& other) const
{
    if (d_type != other.d_type) {
        return false;
    }

    if (d_channel != other.d_channel) {
        return false;
    }

    if (!d_serializedData || !other.d_serializedData) {
        return d_serializedData == other.d_serializedData;
    }

    return *d_serializedData == *other.d_serializedData;
}

bool Frame::operator!=(const Frame& other) const { return !(*this == other); }

} // namespace rmqamqpt
} // namespace BloombergLP
