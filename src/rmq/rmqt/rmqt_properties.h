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

#ifndef INCLUDED_RMQT_PROPERTIES
#define INCLUDED_RMQT_PROPERTIES

#include <rmqt_fieldvalue.h>

#include <bdlb_nullablevalue.h>
#include <bdlt_datetime.h>

#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_set.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

//@PURPOSE: Provide values of different properties for queues or exchanges like
//          durable, auto-delete etc.

namespace Durable {
typedef enum { OFF = 0, ON = 1 } Value;
}

namespace AutoDelete {
typedef enum { OFF = 0, ON = 1 } Value;
}

namespace Internal {
typedef enum { NO = 0, YES = 1 } Value;
}

namespace DeliveryMode {
typedef enum { NON_PERSISTENT = 1, PERSISTENT = 2 } Value;
}

namespace Exclusive {
typedef enum { OFF = 0, ON = 1 } Value;
}

typedef bsl::set<bsl::string> Tunables;

namespace QueueUnused {
typedef enum { ALLOW_IN_USE = 0, IF_UNUSED = 1 } Value;
}

namespace QueueEmpty {
typedef enum { ALLOW_MSG_DELETE = 0, IF_EMPTY = 1 } Value;
}

namespace Mandatory {
typedef enum { DISCARD_UNROUTABLE = 0, RETURN_UNROUTABLE = 1 } Value;
}

/// \brief Properties is an minimal abstraction of the properties one can set on
/// a message
///
/// currently this is 1:1 with AMQP BasicProperties
struct Properties {
    /// MIME content type
    bdlb::NullableValue<bsl::string> contentType;

    /// MIME content encoding
    bdlb::NullableValue<bsl::string> contentEncoding;

    /// message header field table
    bsl::shared_ptr<rmqt::FieldTable> headers;

    /// non-persistent (1) or persistent (2)
    bdlb::NullableValue<bsl::uint8_t> deliveryMode;

    /// message priority, 0 to 9
    bdlb::NullableValue<bsl::uint8_t> priority;

    /// application correlation identifier
    bdlb::NullableValue<bsl::string> correlationId;

    /**
     * FOR APP USE per
     * https://www.rabbitmq.com/resources/specs/amqp0-9-1.extended.xml
     */

    /// address to reply to
    bdlb::NullableValue<bsl::string> replyTo;

    /// message expiration specification
    bdlb::NullableValue<bsl::string> expiration;

    /// application message identifier
    bdlb::NullableValue<bsl::string> messageId;

    /// message timestamp
    bdlb::NullableValue<bdlt::Datetime> timestamp;

    /// message type name
    bdlb::NullableValue<bsl::string> type;

    /// creating user id
    bdlb::NullableValue<bsl::string> userId;

    /// creating application id
    bdlb::NullableValue<bsl::string> appId;

    bsl::ostream&
    print(bsl::ostream& stream, int level, int spacesPerLevel) const;
};

bool operator==(const Properties& lhs, const Properties& rhs);
bool operator!=(const Properties& lhs, const Properties& rhs);
bsl::ostream& operator<<(bsl::ostream& os, const Properties& props);

} // namespace rmqt
} // namespace BloombergLP

#endif
