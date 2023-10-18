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

#include <rmqamqpt_contentheader.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <rmqt_message.h>

#include <ball_log.h>
#include <bsl_cstddef.h>

namespace BloombergLP {
namespace rmqamqpt {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.CONTENTHEADER")
}

ContentHeader::ContentHeader()
: d_classId()
, d_bodySize()
, d_properties()
{
}

ContentHeader::ContentHeader(rmqamqpt::Constants::AMQPClassId classId,
                             uint64_t bodySize,
                             const BasicProperties& props)
: d_classId(classId)
, d_bodySize(bodySize)
, d_properties(props)
{
}

ContentHeader::ContentHeader(rmqamqpt::Constants::AMQPClassId classId,
                             const rmqt::Message& message)
: d_classId(classId)
, d_bodySize(message.payloadSize())
, d_properties(BasicProperties(message.properties()))
{
}

bool ContentHeader::decode(ContentHeader* contentHeader,
                           const uint8_t* data,
                           bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);
    if (sizeof(uint16_t) + sizeof(uint16_t) +
            sizeof(contentHeader->d_bodySize) >
        buffer.available()) {
        BALL_LOG_ERROR << "Not enough data to decode ContentHeader frame. "
                       << "Available buffer bytes: " << buffer.available();
        return false;
    }

    const uint16_t classId = buffer.copy<bdlb::BigEndianUint16>();
    contentHeader->d_classId =
        static_cast<rmqamqpt::Constants::AMQPClassId>(classId);

    const uint16_t weight = buffer.copy<bdlb::BigEndianUint16>();
    // Reserved and should always be zero
    if (weight != 0) {
        BALL_LOG_ERROR
            << "Weight field inside content header frame is non-zero: "
            << weight;
    }
    contentHeader->d_bodySize = buffer.copy<bdlb::BigEndianUint64>();

    return BasicProperties::decode(&contentHeader->d_properties,
                                   static_cast<const uint8_t*>(buffer.ptr()),
                                   buffer.available());
}

void ContentHeader::encode(Writer& output, const ContentHeader& contentHeader)
{
    Types::write(output, bdlb::BigEndianUint16::make(contentHeader.classId()));
    Types::write(output, bdlb::BigEndianUint16::make(0));
    Types::write(output, bdlb::BigEndianUint64::make(contentHeader.bodySize()));
    BasicProperties::encode(output, contentHeader.d_properties);
}

bsl::ostream& operator<<(bsl::ostream& os, const ContentHeader& contentHeader)
{
    os << "ContentHeader = ["
       << "class-id: " << contentHeader.classId()
       << ", weight: " << contentHeader.weight()
       << ", bodysize: " << contentHeader.bodySize()
       << ", properties: " << contentHeader.properties() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
