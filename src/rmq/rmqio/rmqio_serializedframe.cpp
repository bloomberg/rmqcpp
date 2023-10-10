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

#include <rmqio_serializedframe.h>

#include <rmqamqpt_frame.h>

namespace BloombergLP {
namespace rmqio {

SerializedFrame::SerializedFrame(const rmqamqpt::Frame& frame)
: d_length(frame.totalFrameSize())
, d_buffer(frame.serializedData())
{
}

SerializedFrame::SerializedFrame(const bsl::uint8_t* data, bsl::size_t length)
: d_length(length)
, d_buffer(bsl::make_shared<bsl::vector<bsl::uint8_t> >(data, data + length))
{
}

bool SerializedFrame::operator==(const SerializedFrame& other) const
{
    if (d_length != other.d_length) {
        return false;
    }

    if (!d_buffer || !other.d_buffer) {
        return d_buffer == other.d_buffer;
    }

    return *d_buffer == *other.d_buffer;
}

bool SerializedFrame::operator!=(const SerializedFrame& other) const
{
    if (d_length == other.d_length) {
        return false;
    }

    if (!d_buffer || !other.d_buffer) {
        return d_buffer != other.d_buffer;
    }

    return *d_buffer != *other.d_buffer;
}

} // namespace rmqio
} // namespace BloombergLP
