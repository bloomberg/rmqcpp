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

#ifndef INCLUDED_RMQAMQP_MESSAGE
#define INCLUDED_RMQAMQP_MESSAGE

#include <rmqamqpt_heartbeat.h>
#include <rmqamqpt_method.h>
#include <rmqt_message.h>

#include <bdlb_variant.h>

//@PURPOSE: Represent a message decoded from one or more Frames
//
//@CLASSES:
//  rmqamqp::Message: A Variant containing a useful message decoded from
//       one or more frames which can be one or more frame types:
//         Frame Type 1   (Method)         -> message.the<Method>()
//         Frame Type 2/3 (Header/Body(s)) -> message.the<rmqt::Message>()
//         Frame Type 8   (Heartbeat)      -> message.the<Heartbeat>()
//

namespace BloombergLP {
namespace rmqamqp {

typedef bdlb::Variant<rmqamqpt::Heartbeat, rmqt::Message, rmqamqpt::Method>
    Message;

} // namespace rmqamqp
} // namespace BloombergLP

#endif
