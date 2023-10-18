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

#include <rmqamqpt_queueunbind.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bdlb_bigendian.h>

#include <bsl_sstream.h>

namespace BloombergLP {
namespace rmqamqpt {

QueueUnbind::QueueUnbind()
: d_queue()
, d_exchange()
, d_routingKey()
, d_arguments()
{
}

QueueUnbind::QueueUnbind(const bsl::string& queue,
                         const bsl::string& exchange,
                         const bsl::string& routingKey,
                         const rmqt::FieldTable& arguments)
: d_queue(queue)
, d_exchange(exchange)
, d_routingKey(routingKey)
, d_arguments(arguments)
{
}

bool QueueUnbind::decode(QueueUnbind* bind,
                         const uint8_t* data,
                         bsl::size_t dataLength)
{

    rmqamqpt::Buffer buffer(data, dataLength);

    // Skip reserved short
    if (buffer.available() < sizeof(uint16_t)) {
        return false;
    }
    buffer.skip(sizeof(uint16_t));

    if (!Types::decodeShortString(&bind->d_queue, &buffer)) {
        return false;
    }

    if (!Types::decodeShortString(&bind->d_exchange, &buffer)) {
        return false;
    }

    if (!Types::decodeShortString(&bind->d_routingKey, &buffer)) {
        return false;
    }

    return Types::decodeFieldTable(&bind->d_arguments, &buffer);
}

void QueueUnbind::encode(Writer& output, const QueueUnbind& bind)
{
    Types::write(output, bdlb::BigEndianUint16::make(0));
    Types::encodeShortString(output, bind.queue());
    Types::encodeShortString(output, bind.exchange());
    Types::encodeShortString(output, bind.routingKey());

    Types::encodeFieldTable(output, bind.arguments());
}

bool operator==(const QueueUnbind& lhs, const QueueUnbind& rhs)
{
    if (lhs.queue() != rhs.queue()) {
        return false;
    }
    if (lhs.exchange() != rhs.exchange()) {
        return false;
    }
    if (lhs.routingKey() != rhs.routingKey()) {
        return false;
    }
    if (lhs.arguments() != rhs.arguments()) {
        return false;
    }
    return true;
}

bsl::ostream& operator<<(bsl::ostream& os, const QueueUnbind& queueBind)
{
    os << "QueueUnbind = [queue: " << queueBind.queue()
       << ", exchange: " << queueBind.exchange()
       << ", routing-key: " << queueBind.routingKey()
       << ", arguments: " << queueBind.arguments() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
