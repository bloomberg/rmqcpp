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

#ifndef INCLUDED_RMQT_ENVELOPE
#define INCLUDED_RMQT_ENVELOPE

#include <bsl_cstdint.h>
#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief Provide a class that holds additional data about rmqt::Message.
///
///  Holds message delivery information (amqp basic deliver from
///  broker): delivery tag, routingKey, redelivery flag. Additionally provides
///  the id of the channel's lifetime (client) the message belongs to.

class Envelope {
  public:
    /// \brief Envelope constructor. Only to be called by rmqcpp internals.
    ///
    /// \param deliveryTag Message delivery tag
    /// \param channelLifetimeId Identifier of channel lifetime id (increases
    ///        after every reconnection)
    /// \param consumerTag Consumer tag
    /// \param exchange AMQP exchange name
    /// \param routingKey AMQP routing key
    /// \param redelivered True if the message has been redelivered
    /// (https://www.rabbitmq.com/reliability.html#consumer-side)
    Envelope(uint64_t deliveryTag,
             size_t channelLifetimeId,
             const bsl::string& consumerTag,
             const bsl::string& exchange,
             const bsl::string& routingKey,
             bool redelivered);

    /// \brief Message delivery tag
    uint64_t deliveryTag() const { return d_deliveryTag; }

    /// \brief Identifier of channel lifetime id (increases after every
    /// reconnect)
    size_t channelLifetimeId() const { return d_channelLifetimeId; }

    /// \brief Consumer tag
    const bsl::string& consumerTag() const { return d_consumerTag; }

    /// \brief AMQP exchange name
    const bsl::string& exchange() const { return d_exchange; }

    /// \brief AMQP routing key
    const bsl::string& routingKey() const { return d_routingKey; }

    /// \brief True if the message has been redelivered
    bool redelivered() const { return d_redelivered; }

  private:
    uint64_t d_deliveryTag;
    size_t d_channelLifetimeId;
    bsl::string d_consumerTag;
    bsl::string d_exchange;
    bsl::string d_routingKey;
    bool d_redelivered;
};

bsl::ostream& operator<<(bsl::ostream& os, const rmqt::Envelope& envelope);

} // namespace rmqt
} // namespace BloombergLP

#endif
