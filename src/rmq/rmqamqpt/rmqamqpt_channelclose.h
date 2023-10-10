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

#ifndef INCLUDED_RMQAMQPT_CHANNELCLOSE
#define INCLUDED_RMQAMQPT_CHANNELCLOSE

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_iostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide channel CLOSE method
/// This method indicates that the sender wants to close the channel. This may
/// be due to internal conditions (e.g. a forced shut-down) or due to an error
/// handling a specific method, i.e. an exception.

/// When a close is due to an exception, the sender provides the class and
/// method id of the method which caused the exception.

class ChannelClose {
  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CHANNEL_CLOSE;

    ChannelClose();

    ChannelClose(rmqamqpt::Constants::AMQPReplyCode replyCode,
                 const bsl::string& replyText,
                 rmqamqpt::Constants::AMQPClassId classId =
                     rmqamqpt::Constants::NO_CLASS,
                 rmqamqpt::Constants::AMQPMethodId methodId =
                     rmqamqpt::Constants::NO_METHOD);

    size_t encodedSize() const
    {
        return 3 * sizeof(uint16_t) + sizeof(uint8_t) + d_replyText.size();
    }

    rmqamqpt::Constants::AMQPReplyCode replyCode() const { return d_replyCode; }
    bsl::string replyText() const { return d_replyText; }
    rmqamqpt::Constants::AMQPClassId classId() const { return d_classId; }
    rmqamqpt::Constants::AMQPMethodId methodId() const { return d_methodId; }

    static bool
    decode(ChannelClose* start, const uint8_t* data, bsl::size_t dataLength);
    static void encode(Writer& output, const ChannelClose& start);

  private:
    rmqamqpt::Constants::AMQPReplyCode d_replyCode;
    bsl::string d_replyText;

    // Failing method class id
    // Value 0 in case of no errors
    rmqamqpt::Constants::AMQPClassId d_classId;

    // Failing method id
    // Value 0 in case of no errors
    rmqamqpt::Constants::AMQPMethodId d_methodId;
};

bsl::ostream& operator<<(bsl::ostream& os, const ChannelClose& closeMethod);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
