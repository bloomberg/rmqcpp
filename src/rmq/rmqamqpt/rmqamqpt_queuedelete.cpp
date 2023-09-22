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

#include <rmqamqpt_queuedelete.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bdlb_bigendian.h>

#include <bsl_sstream.h>

namespace BloombergLP {
namespace rmqamqpt {
namespace {
const uint8_t BITMASK_BIT_0 = (1 << 0);
const uint8_t BITMASK_BIT_1 = (1 << 1);
const uint8_t BITMASK_BIT_2 = (1 << 2);
} // namespace

QueueDelete::QueueDelete()
: d_name()
, d_ifUnused()
, d_ifEmpty()
, d_noWait()
{
}

QueueDelete::QueueDelete(const bsl::string& name,
                         bool ifUnused,
                         bool ifEmpty,
                         bool noWait)
: d_name(name)
, d_ifUnused(ifUnused)
, d_ifEmpty(ifEmpty)
, d_noWait(noWait)
{
}

bool QueueDelete::decode(QueueDelete* queueDelete,
                         const uint8_t* data,
                         bsl::size_t dataLength)
{

    rmqamqpt::Buffer buffer(data, dataLength);

    // Skip reserved short
    if (buffer.available() < sizeof(uint16_t)) {
        return false;
    }
    buffer.skip(sizeof(uint16_t));

    if (!Types::decodeShortString(&queueDelete->d_name, &buffer) ||
        buffer.available() < sizeof(uint8_t)) {
        return false;
    }

    uint8_t bitmask         = buffer.copy<uint8_t>();
    queueDelete->d_ifUnused = bitmask & BITMASK_BIT_0;
    queueDelete->d_ifEmpty  = bitmask & BITMASK_BIT_1;
    queueDelete->d_noWait   = bitmask & BITMASK_BIT_2;

    return true;
}

void QueueDelete::encode(Writer& output, const QueueDelete& queueDelete)
{
    Types::write(output, bdlb::BigEndianUint16::make(0));
    Types::encodeShortString(output, queueDelete.name());

    uint8_t bitmask = 0;
    if (queueDelete.d_ifUnused) {
        bitmask |= BITMASK_BIT_0;
    }
    if (queueDelete.d_ifEmpty) {
        bitmask |= BITMASK_BIT_1;
    }
    if (queueDelete.d_noWait) {
        bitmask |= BITMASK_BIT_2;
    }

    Types::write(output, bitmask);
}

bool operator==(const QueueDelete& lhs, const QueueDelete& rhs)
{
    if (lhs.name() != rhs.name()) {
        return false;
    }
    if (lhs.ifUnused() != rhs.ifUnused()) {
        return false;
    }
    if (lhs.ifEmpty() != rhs.ifEmpty()) {
        return false;
    }
    if (lhs.noWait() != rhs.noWait()) {
        return false;
    }
    return true;
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueDelete& queueDelete)
{
    os << "QueueDelete = [name: " << queueDelete.name()
       << ", if-unused: " << queueDelete.ifUnused()
       << ", if-empty: " << queueDelete.ifEmpty()
       << ", no-wait: " << queueDelete.noWait() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
