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

#include <rmqamqpt_queuedeclareok.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bdlb_bigendian.h>

namespace BloombergLP {
namespace rmqamqpt {

QueueDeclareOk::QueueDeclareOk()
: d_name()
, d_messageCount()
, d_consumerCount()
{
}

QueueDeclareOk::QueueDeclareOk(const bsl::string& name,
                               bsl::uint32_t messageCount,
                               bsl::uint32_t consumerCount)
: d_name(name)
, d_messageCount(bdlb::BigEndianUint32::make(messageCount))
, d_consumerCount(bdlb::BigEndianUint32::make(consumerCount))
{
}

bool QueueDeclareOk::decode(QueueDeclareOk* declareOk,
                            const uint8_t* data,
                            bsl::size_t dataLength)
{

    rmqamqpt::Buffer buffer(data, dataLength);

    if (!Types::decodeShortString(&declareOk->d_name, &buffer) ||
        buffer.available() < sizeof(declareOk->d_messageCount) +
                                 sizeof(declareOk->d_consumerCount)) {
        return false;
    }

    declareOk->d_messageCount  = buffer.copy<bdlb::BigEndianUint32>();
    declareOk->d_consumerCount = buffer.copy<bdlb::BigEndianUint32>();

    return true;
}

void QueueDeclareOk::encode(Writer& output, const QueueDeclareOk& declareOk)
{
    Types::encodeShortString(output, declareOk.name());
    Types::write(output, bdlb::BigEndianUint32::make(declareOk.messageCount()));
    Types::write(output,
                 bdlb::BigEndianUint32::make(declareOk.consumerCount()));
}

bool operator==(const QueueDeclareOk& lhs, const QueueDeclareOk& rhs)
{
    if (lhs.name() != rhs.name()) {
        return false;
    }
    if (lhs.messageCount() != rhs.messageCount()) {
        return false;
    }
    if (lhs.consumerCount() != rhs.consumerCount()) {
        return false;
    }
    return true;
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueDeclareOk& queueDeclareOk)
{
    os << "QueueDeclareOk = [name: " << queueDeclareOk.name()
       << ", message-count: " << queueDeclareOk.messageCount()
       << ", consumer-count: " << queueDeclareOk.consumerCount() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
