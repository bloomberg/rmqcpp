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

#include <rmqamqpt_exchangedeclare.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

#include <bdlb_bigendian.h>

#include <bsl_sstream.h>

namespace BloombergLP {
namespace rmqamqpt {
namespace {
const uint8_t BITMASK_BIT_PASSIVE    = (1 << 0);
const uint8_t BITMASK_BIT_DURABLE    = (1 << 1);
const uint8_t BITMASK_BIT_AUTODELETE = (1 << 2);
const uint8_t BITMASK_BIT_INTERNAL   = (1 << 3);
const uint8_t BITMASK_BIT_NOWAIT     = (1 << 4);
} // namespace

ExchangeDeclare::ExchangeDeclare()
: d_name()
, d_type()
, d_passive()
, d_durable()
, d_autoDelete()
, d_internal()
, d_noWait()
, d_arguments()
{
}

ExchangeDeclare::ExchangeDeclare(
    const bsl::string& name,
    const bsl::string& type,
    bool passive                      = false,
    bool durable                      = true,
    bool autoDelete                   = false,
    bool internal                     = false,
    bool noWait                       = false,
    const rmqt::FieldTable& arguments = rmqt::FieldTable())
: d_name(name)
, d_type(type)
, d_passive(passive)
, d_durable(durable)
, d_autoDelete(autoDelete)
, d_internal(internal)
, d_noWait(noWait)
, d_arguments(arguments)
{
}

bool ExchangeDeclare::decode(ExchangeDeclare* declare,
                             const uint8_t* data,
                             bsl::size_t dataLength)
{

    rmqamqpt::Buffer buffer(data, dataLength);

    // Skip reserved short
    if (buffer.available() < sizeof(uint16_t)) {
        return false;
    }
    buffer.skip(sizeof(uint16_t));

    if (!Types::decodeShortString(&declare->d_name, &buffer)) {
        return false;
    }

    if (!Types::decodeShortString(&declare->d_type, &buffer)) {
        return false;
    }

    if (buffer.available() <
        sizeof(declare->d_passive) + sizeof(declare->d_durable) +
            sizeof(declare->d_autoDelete) + sizeof(declare->d_internal) +
            sizeof(declare->d_noWait)) {
        return false;
    }

    uint8_t bitmask       = buffer.copy<uint8_t>();
    declare->d_passive    = bitmask & BITMASK_BIT_PASSIVE;
    declare->d_durable    = bitmask & BITMASK_BIT_DURABLE;
    declare->d_autoDelete = bitmask & BITMASK_BIT_AUTODELETE;
    declare->d_internal   = bitmask & BITMASK_BIT_INTERNAL;
    declare->d_noWait     = bitmask & BITMASK_BIT_NOWAIT;

    return Types::decodeFieldTable(&declare->d_arguments, &buffer);
}

void ExchangeDeclare::encode(Writer& output, const ExchangeDeclare& declare)
{
    Types::write(output, bdlb::BigEndianUint16::make(0));
    Types::encodeShortString(output, declare.name());
    Types::encodeShortString(output, declare.type());

    uint8_t bitmask = 0;
    if (declare.d_passive) {
        bitmask |= BITMASK_BIT_PASSIVE;
    }
    if (declare.d_durable) {
        bitmask |= BITMASK_BIT_DURABLE;
    }
    if (declare.d_autoDelete) {
        bitmask |= BITMASK_BIT_AUTODELETE;
    }
    if (declare.d_internal) {
        bitmask |= BITMASK_BIT_INTERNAL;
    }
    if (declare.d_noWait) {
        bitmask |= BITMASK_BIT_NOWAIT;
    }

    Types::write(output, bitmask);

    Types::encodeFieldTable(output, declare.arguments());
}

bool operator==(const ExchangeDeclare& lhs, const ExchangeDeclare& rhs)
{
    return (&lhs == &rhs) ||
           (lhs.name() == rhs.name() && lhs.type() == rhs.type() &&
            lhs.passive() == rhs.passive() && lhs.durable() == rhs.durable() &&
            lhs.autoDelete() == rhs.autoDelete() &&
            lhs.internal() == rhs.internal() && lhs.noWait() == rhs.noWait() &&
            lhs.arguments() == rhs.arguments());
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const ExchangeDeclare& exchangeDeclare)
{
    os << "ExchangeDeclare = [name: " << exchangeDeclare.name()
       << ", type: " << exchangeDeclare.type()
       << ", passive: " << exchangeDeclare.passive()
       << ", durable: " << exchangeDeclare.durable()
       << ", auto-delete: " << exchangeDeclare.autoDelete()
       << ", internal: " << exchangeDeclare.internal()
       << ", no-wait: " << exchangeDeclare.noWait()
       << ", arguments: " << exchangeDeclare.arguments() << "]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
