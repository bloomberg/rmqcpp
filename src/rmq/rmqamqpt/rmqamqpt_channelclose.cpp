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

#include <rmqamqpt_channelclose.h>

#include <rmqamqpt_types.h>

#include <rmqamqpt_buffer.h>

#include <bdlb_bigendian.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

ChannelClose::ChannelClose()
: d_replyCode()
, d_replyText()
, d_classId()
, d_methodId()
{
}

ChannelClose::ChannelClose(rmqamqpt::Constants::AMQPReplyCode replyCode,
                           const bsl::string& replyText,
                           rmqamqpt::Constants::AMQPClassId classId,
                           rmqamqpt::Constants::AMQPMethodId methodId)
: d_replyCode(replyCode)
, d_replyText(replyText)
, d_classId(classId)
, d_methodId(methodId)
{
}

bool ChannelClose::decode(ChannelClose* close,
                          const uint8_t* data,
                          bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);
    if (buffer.available() < sizeof(bdlb::BigEndianUint16)) {
        return false;
    }

    const uint16_t replyCode = buffer.copy<bdlb::BigEndianUint16>();
    close->d_replyCode =
        static_cast<rmqamqpt::Constants::AMQPReplyCode>(replyCode);

    if (!Types::decodeShortString(&close->d_replyText, &buffer)) {
        return false;
    }

    if (buffer.available() < 2 * sizeof(bdlb::BigEndianUint16)) {
        return false;
    }

    const uint16_t classId = buffer.copy<bdlb::BigEndianUint16>();
    close->d_classId = static_cast<rmqamqpt::Constants::AMQPClassId>(classId);

    const uint16_t methodId = buffer.copy<bdlb::BigEndianUint16>();
    close->d_methodId =
        static_cast<rmqamqpt::Constants::AMQPMethodId>(methodId);

    return true;
}

void ChannelClose::encode(Writer& output, const ChannelClose& close)
{
    Types::write(output, bdlb::BigEndianUint16::make(close.replyCode()));

    Types::encodeShortString(output, close.replyText());

    Types::write(output, bdlb::BigEndianUint16::make(close.classId()));
    Types::write(output, bdlb::BigEndianUint16::make(close.methodId()));
}

bsl::ostream& operator<<(bsl::ostream& os, const ChannelClose& closeMethod)
{
    os << "Channel Close = [replyCode: " << closeMethod.replyCode()
       << ", replyText: \"" << closeMethod.replyText() << "\""
       << ", classId: " << closeMethod.classId()
       << ", methodId: " << closeMethod.methodId() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
