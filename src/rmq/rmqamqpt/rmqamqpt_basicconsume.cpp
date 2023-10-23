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

#include <rmqamqpt_basicconsume.h>

#include <rmqamqpt_types.h>

#include <rmqamqpt_buffer.h>

#include <bdlb_bigendian.h>

#include <bsl_cstddef.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqamqpt {
namespace {
const uint8_t MASK_BIT_0 = 1 << 0;
const uint8_t MASK_BIT_1 = 1 << 1;
const uint8_t MASK_BIT_2 = 1 << 2;
const uint8_t MASK_BIT_3 = 1 << 3;
} // namespace

BasicConsume::BasicConsume(const bsl::string& queue,
                           const bsl::string& consumerTag,
                           const rmqt::FieldTable& arguments,
                           bool noLocal,
                           bool noAck,
                           bool exclusive,
                           bool noWait)
: d_queue(queue)
, d_consumerTag(consumerTag)
, d_noLocal(noLocal)
, d_noAck(noAck)
, d_exclusive(exclusive)
, d_noWait(noWait)
, d_arguments(arguments)
{
}

BasicConsume::BasicConsume()
: d_queue()
, d_consumerTag()
, d_noLocal()
, d_noAck()
, d_exclusive()
, d_noWait()
, d_arguments()
{
}

bool BasicConsume::decode(BasicConsume* consume,
                          const uint8_t* data,
                          bsl::size_t dataLength)
{
    rmqamqpt::Buffer buffer(data, dataLength);

    // uint16_t reserved1;
    if (sizeof(uint16_t /* reserved1 */) > buffer.available()) {
        return false;
    }
    /*reserved1 = */ buffer.copy<bdlb::BigEndianUint16>();
    if (!Types::decodeShortString(&consume->d_queue, &buffer)) {
        return false;
    }
    if (!Types::decodeShortString(&consume->d_consumerTag, &buffer)) {
        return false;
    }
    if (sizeof(uint8_t) > buffer.available()) {
        return false;
    }

    const uint8_t theByte = buffer.copy<uint8_t>();

    consume->d_noLocal   = theByte & MASK_BIT_0;
    consume->d_noAck     = theByte & MASK_BIT_1;
    consume->d_exclusive = theByte & MASK_BIT_2;
    consume->d_noWait    = theByte & MASK_BIT_3;

    return Types::decodeFieldTable(&consume->d_arguments, &buffer);
}

void BasicConsume::encode(Writer& output, const BasicConsume& consume)
{
    Types::write(output, bdlb::BigEndianUint16::make(0 /* reserved1 */));
    Types::encodeShortString(output, consume.d_queue);
    Types::encodeShortString(output, consume.d_consumerTag);

    uint8_t theByte = 0;

    if (consume.d_noLocal) {
        theByte |= MASK_BIT_0;
    }
    if (consume.d_noAck) {
        theByte |= MASK_BIT_1;
    }
    if (consume.d_exclusive) {
        theByte |= MASK_BIT_2;
    }
    if (consume.d_noWait) {
        theByte |= MASK_BIT_3;
    }

    Types::write(output, theByte);

    Types::encodeFieldTable(output, consume.d_arguments);
}

bsl::ostream& operator<<(bsl::ostream& os, const BasicConsume& consume)
{
    return os << "BasicConsume: [ queue: " << consume.queue()
              << ", consumer-tag: " << consume.consumerTag()
              << ", no-local: " << consume.noLocal()
              << ", no-ack: " << consume.noAck()
              << ", exclusive: " << consume.exclusive()
              << ", no-wait: " << consume.noWait()
              << ", arguments: " << consume.arguments() << "]";
}

bool operator==(const BasicConsume& lhs, const BasicConsume& rhs)
{
    return (&lhs == &rhs) ||
           (lhs.queue() == rhs.queue() &&
            lhs.consumerTag() == rhs.consumerTag() &&
            lhs.noLocal() == rhs.noLocal() && lhs.noAck() == rhs.noAck() &&
            lhs.exclusive() == rhs.exclusive() &&
            lhs.noWait() == rhs.noWait() && lhs.arguments() == rhs.arguments());
}

} // namespace rmqamqpt
} // namespace BloombergLP
