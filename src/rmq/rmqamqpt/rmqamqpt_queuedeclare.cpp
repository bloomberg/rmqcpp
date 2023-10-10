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

#include <rmqamqpt_queuedeclare.h>

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
const uint8_t BITMASK_BIT_3 = (1 << 3);
const uint8_t BITMASK_BIT_4 = (1 << 4);
} // namespace

QueueDeclare::QueueDeclare()
: d_name()
, d_passive()
, d_durable()
, d_exclusive()
, d_autoDelete()
, d_noWait()
, d_arguments()
{
}

QueueDeclare::QueueDeclare(const bsl::string& name,
                           bool passive,
                           bool durable,
                           bool exclusive,
                           bool autoDelete,
                           bool noWait,
                           const rmqt::FieldTable& arguments)
: d_name(name)
, d_passive(passive)
, d_durable(durable)
, d_exclusive(exclusive)
, d_autoDelete(autoDelete)
, d_noWait(noWait)
, d_arguments(arguments)
{
}

bool QueueDeclare::decode(QueueDeclare* declare,
                          const uint8_t* data,
                          bsl::size_t dataLength)
{

    rmqamqpt::Buffer buffer(data, dataLength);

    // Skip reserved short
    if (buffer.available() < sizeof(uint16_t)) {
        return false;
    }
    buffer.skip(sizeof(uint16_t));

    if (!Types::decodeShortString(&declare->d_name, &buffer) ||
        buffer.available() <
            sizeof(declare->d_passive) + sizeof(declare->d_durable) +
                sizeof(declare->d_exclusive) + sizeof(declare->d_autoDelete) +
                sizeof(declare->d_noWait)) {
        return false;
    }

    uint8_t bitmask       = buffer.copy<uint8_t>();
    declare->d_passive    = bitmask & BITMASK_BIT_0;
    declare->d_durable    = bitmask & BITMASK_BIT_1;
    declare->d_exclusive  = bitmask & BITMASK_BIT_2;
    declare->d_autoDelete = bitmask & BITMASK_BIT_3;
    declare->d_noWait     = bitmask & BITMASK_BIT_4;

    return Types::decodeFieldTable(&declare->d_arguments, &buffer);
}

void QueueDeclare::encode(Writer& output, const QueueDeclare& declare)
{
    Types::write(output, bdlb::BigEndianUint16::make(0));
    Types::encodeShortString(output, declare.name());

    uint8_t bitmask = 0;
    if (declare.d_passive) {
        bitmask |= BITMASK_BIT_0;
    }
    if (declare.d_durable) {
        bitmask |= BITMASK_BIT_1;
    }
    if (declare.d_exclusive) {
        bitmask |= BITMASK_BIT_2;
    }
    if (declare.d_autoDelete) {
        bitmask |= BITMASK_BIT_3;
    }
    if (declare.d_noWait) {
        bitmask |= BITMASK_BIT_4;
    }

    Types::write(output, bitmask);

    Types::encodeFieldTable(output, declare.arguments());
}

bool operator==(const QueueDeclare& lhs, const QueueDeclare& rhs)
{
    if (lhs.name() != rhs.name()) {
        return false;
    }
    if (lhs.passive() != rhs.passive()) {
        return false;
    }
    if (lhs.durable() != rhs.durable()) {
        return false;
    }
    if (lhs.exclusive() != rhs.exclusive()) {
        return false;
    }
    if (lhs.autoDelete() != rhs.autoDelete()) {
        return false;
    }
    if (lhs.noWait() != rhs.noWait()) {
        return false;
    }
    if (lhs.arguments() != rhs.arguments()) {
        return false;
    }
    return true;
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueDeclare& queueDeclare)
{
    os << "QueueDeclare = [name: " << queueDeclare.name()
       << ", passive: " << queueDeclare.passive()
       << ", durable: " << queueDeclare.durable()
       << ", exclusive: " << queueDeclare.exclusive()
       << ", auto-delete: " << queueDeclare.autoDelete()
       << ", no-wait: " << queueDeclare.noWait()
       << ", arguments: " << queueDeclare.arguments() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
