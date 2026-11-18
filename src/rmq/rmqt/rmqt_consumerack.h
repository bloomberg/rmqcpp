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

#ifndef INCLUDED_RMQT_CONSUMERACK
#define INCLUDED_RMQT_CONSUMERACK

#include <rmqt_envelope.h>

#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqt {

class ConsumerAck {
  public:
    /// How a consumer resolved a delivery.
    ///
    /// Each value names the AMQP method it sends and the value of that
    /// method's `requeue` flag. Only `basic.reject` counts the delivery as a
    /// failed attempt; `basic.nack` does not.
    ///
    /// Values are fixed and must not be renumbered.
    ///
    /// \see https://www.rabbitmq.com/docs/nack
    /// \see
    /// https://www.rabbitmq.com/docs/quorum-queues#when-is-delivery-count-incremented
    enum Type {
        /// `basic.ack`
        ACK = 0,
        /// `basic.nack` with requeue=false
        NACK_REQUEUE_FALSE = 1,
        /// `basic.nack` with requeue=true
        NACK_REQUEUE_TRUE = 2,
        /// `basic.reject` with requeue=false
        REJECT_REQUEUE_FALSE = 3,
        /// `basic.reject` with requeue=true
        REJECT_REQUEUE_TRUE = 4,

        /// \deprecated Use `NACK_REQUEUE_FALSE`. This name predates support
        ///             for `basic.reject` and is misleading: it sends
        ///             `basic.nack`, so the delivery is not counted as a
        ///             failed attempt.
        REJECT = NACK_REQUEUE_FALSE,
        /// \deprecated Use `NACK_REQUEUE_TRUE`. This name predates support
        ///             for `basic.reject`.
        REQUEUE = NACK_REQUEUE_TRUE
    };

    ConsumerAck(const Envelope& envelope, Type type);
    ~ConsumerAck();

    const Envelope& envelope() const { return d_envelope; }
    Type type() const { return d_type; }

    /// RETURN `true` if `type` sends `basic.reject` rather than
    /// `basic.nack`. Note this is `false` for the deprecated `REJECT`, which
    /// sends `basic.nack`.
    static bool usesBasicReject(Type type);

    /// RETURN `true` if the AMQP method that `type` sends carries a
    /// `multiple` flag, and so may resolve every unresolved delivery up to
    /// and including its delivery tag in one frame.
    ///
    /// `basic.ack` and `basic.nack` carry it, `basic.reject` does not, so a
    /// reject is always sent one frame per delivery. Combining rejects would
    /// resolve only the highest delivery tag and leave the rest outstanding.
    ///
    /// \see https://www.rabbitmq.com/amqp-0-9-1-reference#basic.reject
    static bool supportsMultiple(Type type);

  private:
    Envelope d_envelope;
    Type d_type;
};

///  Stream out
bsl::ostream& operator<<(bsl::ostream& os, ConsumerAck::Type type);

} // namespace rmqt
} // namespace BloombergLP

#endif
