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

#ifndef INCLUDED_RMQAMQP_MESSAGEWITHROUTE
#define INCLUDED_RMQAMQP_MESSAGEWITHROUTE

#include <rmqt_exchange.h>
#include <rmqt_message.h>

#include <bsls_timeinterval.h>

#include <bsl_cstddef.h>
#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqamqp {

class MessageWithRoute {
  public:
    MessageWithRoute();
    MessageWithRoute(const rmqt::Message& msg,
                     const bsl::string& routingKey,
                     rmqt::Mandatory::Value mandatory);

    const rmqt::Message& message() const { return d_message; }
    const bsl::string& routingKey() const { return d_routingKey; }
    rmqt::Mandatory::Value mandatory() const { return d_mandatory; }

    const bdlb::Guid guid() const { return d_message.guid(); }
    bsl::size_t payloadSize() const { return d_message.payloadSize(); }

  private:
    rmqt::Message d_message;
    bsl::string d_routingKey;
    rmqt::Mandatory::Value d_mandatory;
}; // class MessageWithRoute

bsl::ostream& operator<<(bsl::ostream& os,
                         const rmqamqp::MessageWithRoute& message);

} // namespace rmqamqp
} // namespace BloombergLP

#endif
