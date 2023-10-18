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

#ifndef INCLUDED_RMQAMQPT_BASICPROPERTIES
#define INCLUDED_RMQAMQPT_BASICPROPERTIES

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <rmqt_fieldvalue.h>
#include <rmqt_properties.h>
#include <rmqt_shortstring.h>

#include <bdlb_variant.h>
#include <bdlt_datetime.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_optional.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Represent basic fields for a message:
/// https://www.rabbitmq.com/amqp-0-9-1-reference.html#class.basic

class BasicProperties {
  public:
    typedef enum {
        CONTENT_TYPE = 0,
        CONTENT_ENCODING,
        HEADERS,
        DELIVERY_MODE,
        PRIORITY,
        CORRELATION_ID,
        REPLY_TO,
        EXPIRATION,
        MESSAGE_ID,
        TIMESTAMP,
        TYPE,
        USER_ID,
        APP_ID,
        RESERVED,

        NUM_BASIC_PROPERTIES

    } Id;

  private:
    rmqt::Properties d_properties;

  public:
    BasicProperties();

    explicit BasicProperties(const rmqt::Properties&);

    uint16_t propertyFlags() const;

    size_t encodedSize() const;

    /// MIME content type
    bsl::optional<bsl::string> contentType() const;

    /// MIME content encoding
    bsl::optional<bsl::string> contentEncoding() const;

    /// message header field table
    bsl::optional<bsl::shared_ptr<rmqt::FieldTable> > headers() const;

    /// non-persistent (1) or persistent (2)
    bsl::optional<bsl::uint8_t> deliveryMode() const;

    /// message priority, 0 to 9
    bsl::optional<bsl::uint8_t> priority() const;

    /// application correlation identifier
    bsl::optional<bsl::string> correlationId() const;

    /**
     * FOR APP USE per
     * https://www.rabbitmq.com/resources/specs/amqp0-9-1.extended.xml
     */

    /// address to reply to
    bsl::optional<bsl::string> replyTo() const;

    /// message expiration specification
    bsl::optional<bsl::string> expiration() const;

    /// application message identifier
    bsl::optional<bsl::string> messageId() const;

    /// message timestamp
    bsl::optional<bdlt::Datetime> timestamp() const;

    /// message type name
    bsl::optional<bsl::string> type() const;

    /// creating user id
    bsl::optional<bsl::string> userId() const;

    /// creating application id
    bsl::optional<bsl::string> appId() const;

    rmqt::Properties toProperties() const;

    void setProperties(const rmqt::Properties& properties);

    static bool decode(BasicProperties*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const BasicProperties&);

    friend bsl::ostream& operator<<(bsl::ostream& os, const BasicProperties&);
};

bsl::ostream& operator<<(bsl::ostream& os, const BasicProperties::Id&);
bsl::ostream& operator<<(bsl::ostream& os, const BasicProperties&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
