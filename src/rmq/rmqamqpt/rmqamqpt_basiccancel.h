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

#ifndef INCLUDED_RMQAMQPT_BASICCANCEL
#define INCLUDED_RMQAMQPT_BASICCANCEL

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide basic CANCEL method
///
/// This method cancels a consumer. This does not affect already delivered
/// messages, but it does mean the server will not send any more messages
/// for that consumer. The client may receive an arbitrary number of
/// messages in between sending the cancel method and receiving the
/// cancel-ok reply.

/// It may also be sent from the server to the client in the event of the
/// consumer being unexpectedly cancelled (i.e. cancelled for any reason
/// other than the server receiving the corresponding basic.cancel from the
/// client). This allows clients to be notified of the loss of consumers due
/// to events such as queue deletion.

/// Note that as it is not a MUST for clients to accept this method from the
/// server, it is advisable for the broker to be able to identify those
/// clients that are capable of accepting the method, through some means of
/// capability negotiation.

class BasicCancel {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::BASIC_CANCEL;

    explicit BasicCancel(const bsl::string& consumerTag, bool noWait = false);
    BasicCancel();

    size_t encodedSize() const
    {
        return 2 * sizeof(uint8_t) + d_consumerTag.size();
    }

    // Identifier for the consumer, valid within the current channel.
    const bsl::string& consumerTag() const { return d_consumerTag; }

    // do not send reply method
    bool noWait() const { return d_noWait; }

    static bool decode(BasicCancel*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicCancel&);

  private:
    bsl::string d_consumerTag;
    bool d_noWait;
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicCancel&);

bool operator==(const BasicCancel&, const BasicCancel&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
