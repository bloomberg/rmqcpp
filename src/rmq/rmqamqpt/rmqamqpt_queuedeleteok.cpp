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

#include <rmqamqpt_queuedeleteok.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bdlb_bigendian.h>

namespace BloombergLP {
namespace rmqamqpt {

QueueDeleteOk::QueueDeleteOk()
: d_messageCount()
{
}

QueueDeleteOk::QueueDeleteOk(bsl::uint32_t messageCount)
: d_messageCount(bdlb::BigEndianUint32::make(messageCount))
{
}

bool QueueDeleteOk::decode(QueueDeleteOk* deleteOk,
                           const uint8_t* data,
                           bsl::size_t dataLength)
{

    rmqamqpt::Buffer buffer(data, dataLength);

    if (buffer.available() < sizeof(deleteOk->d_messageCount)) {
        return false;
    }

    deleteOk->d_messageCount = buffer.copy<bdlb::BigEndianUint32>();

    return true;
}

void QueueDeleteOk::encode(Writer& output, const QueueDeleteOk& deleteOk)
{
    Types::write(output, bdlb::BigEndianUint32::make(deleteOk.messageCount()));
}

bool operator==(const QueueDeleteOk& lhs, const QueueDeleteOk& rhs)
{
    if (lhs.messageCount() != rhs.messageCount()) {
        return false;
    }
    return true;
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueDeleteOk& queueDeleteOk)
{
    os << "QueueDeleteOk = [message-count: " << queueDeleteOk.messageCount()
       << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
