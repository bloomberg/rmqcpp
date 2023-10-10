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

#include <rmqamqp_messagewithroute.h>

#include <rmqt_message.h>

#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqp {

MessageWithRoute::MessageWithRoute()
: d_message()
, d_routingKey()
, d_mandatory(rmqt::Mandatory::RETURN_UNROUTABLE)
{
}

MessageWithRoute::MessageWithRoute(const rmqt::Message& msg,
                                   const bsl::string& routingKey,
                                   rmqt::Mandatory::Value mandatory)
: d_message(msg)
, d_routingKey(routingKey)
, d_mandatory(mandatory)
{
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const rmqamqp::MessageWithRoute& message)
{
    const char* mandatoryFlag =
        (message.mandatory() == rmqt::Mandatory::DISCARD_UNROUTABLE
             ? "DISCARD_UNROUTABLE"
             : "RETURN_UNROUTABLE");

    os << "Message = [ "
       << "guid: " << message.guid()
       << ", message-size: " << message.payloadSize()
       << ", routing-key: " << message.routingKey()
       << ", mandatory: " << mandatoryFlag << " ]";
    return os;
}

} // namespace rmqamqp
} // namespace BloombergLP
