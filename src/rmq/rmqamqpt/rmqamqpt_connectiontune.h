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

#ifndef INCLUDED_RMQAMQPT_CONNECTIONTUNE
#define INCLUDED_RMQAMQPT_CONNECTIONTUNE

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide connection TUNE method
///
/// This method proposes a set of connection configuration values to the client.
/// The client can accept and/or adjust these.

class ConnectionTune {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CONNECTION_TUNE;

    ConnectionTune();

    ConnectionTune(uint16_t channelMax,
                   uint32_t frameMax,
                   uint16_t heartbeatInterval);

    size_t encodedSize() const
    {
        return 2 * sizeof(uint16_t) + sizeof(uint32_t);
    }

    uint16_t channelMax() const { return d_channelMax; }

    uint32_t frameMax() const { return d_frameMax; }

    uint16_t heartbeatInterval() const { return d_heartbeatInterval; }

    static bool
    decode(ConnectionTune* tune, const uint8_t* data, bsl::size_t dataLength);
    static void encode(Writer& output, const ConnectionTune& tune);

  private:
    // https://github.com/rabbitmq/rabbitmq-server/issues/1593
    uint16_t d_channelMax;
    uint32_t d_frameMax;
    uint16_t d_heartbeatInterval;
};

bsl::ostream& operator<<(bsl::ostream& os, const ConnectionTune& tuneMethod);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
