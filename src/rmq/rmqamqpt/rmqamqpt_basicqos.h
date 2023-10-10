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

#ifndef INCLUDED_RMQAMQPT_BASICQOS
#define INCLUDED_RMQAMQPT_BASICQOS

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide basic QOS method
///
/// This method requests a specific quality of service. The QoS can be
/// specified for the current channel or for all channels on the connection.
///
/// The particular properties and semantics of a qos method always depend on
/// the content class semantics. Though the qos method could in principle
/// apply to both peers, it is currently meaningful only for the server.

class BasicQoS {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_QOS;

    explicit BasicQoS(bsl::uint16_t prefetchCount,
                      bsl::uint32_t prefetchSize = 0,
                      bool global                = false);

    BasicQoS();

    size_t encodedSize() const
    {
        return sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t);
    }

    /// The client can request that messages be sent in advance so that when the
    /// client finishes processing a message, the following message is already
    /// held locally, rather than needing to be sent down the channel.
    /// Prefetching gives a performance improvement. This field specifies the
    /// prefetch window size in octets. The server will send a message in
    /// advance if it is equal to or smaller in size than the available prefetch
    /// size (and also falls into other prefetch limits). May be set to zero,
    /// meaning "no specific limit", although other prefetch limits may still
    /// apply. The prefetch-size is ignored if the no-ack option is set.
    bsl::uint32_t prefetchSize() const { return d_prefetchSize; }

    /// Specifies a prefetch window in terms of whole messages. This field may
    /// be used in combination with the prefetch-size field; a message will only
    /// be sent in advance if both prefetch windows (and those at the channel
    /// and connection level) allow it. The prefetch-count is ignored if the
    /// no-ack option is set.
    bsl::uint16_t prefetchCount() const { return d_prefetchCount; }

    /// RabbitMQ has reinterpreted this field. The original specification said:
    /// "By default the QoS settings apply to the current channel only. If this
    /// field is set, they are applied to the entire connection." Instead,
    /// RabbitMQ takes global=false to mean that the QoS settings should apply
    /// per-consumer (for new consumers on the channel; existing ones being
    /// unaffected) and global=true to mean that the QoS settings should apply
    /// per-channel.
    bool global() const { return d_global; }

    static bool decode(BasicQoS*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicQoS&);

  private:
    uint32_t d_prefetchSize;
    uint16_t d_prefetchCount;
    bool d_global;
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicQoS&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
