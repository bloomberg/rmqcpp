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

// rmqamqpt_contentheader.h
#ifndef INCLUDED_RMQAMQPT_CONTENTHEADER
#define INCLUDED_RMQAMQPT_CONTENTHEADER

#include <rmqamqpt_basicproperties.h>
#include <rmqamqpt_constants.h>
#include <rmqamqpt_fieldvalue.h>
#include <rmqamqpt_writer.h>

#include <rmqt_message.h>

#include <bdlb_bigendian.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_optional.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief  Represents, and generates content header frames on request
/// https://www.rabbitmq.com/resources/specs/amqp0-9-1.pdf (4.2.6.1)

class ContentHeader {
  public:
    ContentHeader();

    ContentHeader(rmqamqpt::Constants::AMQPClassId classId,
                  uint64_t bodySize,
                  const BasicProperties& props);

    /// Constructs a ContentHeader by reading size, and BasicProperties from
    /// the message
    ContentHeader(rmqamqpt::Constants::AMQPClassId classId,
                  const rmqt::Message& message);

    size_t encodedSize() const
    {
        return 2 * sizeof(uint16_t) + sizeof(uint64_t) +
               d_properties.encodedSize();
    }

    rmqamqpt::Constants::AMQPClassId classId() const { return d_classId; }

    uint64_t bodySize() const { return d_bodySize; }

    uint16_t weight() const { return 0; }

    const BasicProperties& properties() const { return d_properties; }

    static bool decode(ContentHeader* contentHeader,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const ContentHeader& contentHeader);

  private:
    uint16_t propertyFlags() const { return d_properties.propertyFlags(); }

    rmqamqpt::Constants::AMQPClassId d_classId;
    // Represent AMQP class Id. At the moment, mostly BASIC (60)

    uint64_t d_bodySize;
    // Represent the total size of the content body

    BasicProperties d_properties;
    // https://www.rabbitmq.com/resources/specs/amqp0-9-1.extended.xml
};

bsl::ostream& operator<<(bsl::ostream& os, const ContentHeader& contentHeader);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
