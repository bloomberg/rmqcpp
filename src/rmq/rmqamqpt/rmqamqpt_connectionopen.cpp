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

#include <rmqamqpt_connectionopen.h>

#include <rmqamqpt_buffer.h>
#include <rmqamqpt_types.h>

namespace BloombergLP {
namespace rmqamqpt {

ConnectionOpen::ConnectionOpen(const bsl::string& virtualHost /* = "/" */)
: d_virtualHost(virtualHost)
{
}

bool ConnectionOpen::decode(ConnectionOpen* open,
                            const uint8_t* data,
                            bsl::size_t dataLength)
{
    rmqamqpt::Buffer buff(data, dataLength);

    // Here we do not decode the reserved slots:
    // https://www.rabbitmq.com/amqp-0-9-1-reference.html#connection.open.reserved-1
    return Types::decodeShortString(&open->d_virtualHost, &buff);
}

void ConnectionOpen::encode(Writer& output, const ConnectionOpen& open)
{
    Types::encodeShortString(output, open.virtualHost());

    const bsl::string reserved;
    Types::encodeShortString(output, reserved);

    const uint8_t reserved2 = 0;
    Types::write(output, reserved2);
}

bsl::ostream& operator<<(bsl::ostream& os, const ConnectionOpen& openMethod)
{
    os << "Connection Open = [virtualHost: \"" << openMethod.virtualHost()
       << "\"]";
    return os;
}

} // namespace rmqamqpt
} // namespace BloombergLP
