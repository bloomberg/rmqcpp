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

#ifndef INCLUDED_RMQAMQPT_CONNECTIONTUNEOK
#define INCLUDED_RMQAMQPT_CONNECTIONTUNEOK

#include <bdlb_bigendian.h>
#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstdint.h>
#include <bsl_cstdlib.h>
#include <bsl_iostream.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide connection TUNE-OK method
///
/// This method sends the client's connection tuning parameters to the server.
/// Certain fields are negotiated, others provide capability information.

class ConnectionTuneOk {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CONNECTION_TUNEOK;

    ConnectionTuneOk();

    ConnectionTuneOk(uint16_t channelMax,
                     uint32_t frameMax,
                     uint16_t heartbeatInterval);

    size_t encodedSize() const
    {
        return 2 * sizeof(uint16_t) + sizeof(uint32_t);
    }

    uint16_t channelMax() const { return d_channelMax; }

    uint32_t frameMax() const { return d_frameMax; }

    uint16_t heartbeatInterval() const { return d_heartbeatInterval; }

    static bool decode(ConnectionTuneOk* tuneOk,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const ConnectionTuneOk& tuneOk);

  private:
    uint16_t d_channelMax;
    uint32_t d_frameMax;
    uint16_t d_heartbeatInterval;
};

bsl::ostream& operator<<(bsl::ostream& os,
                         const ConnectionTuneOk& tuneOkMethod);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
