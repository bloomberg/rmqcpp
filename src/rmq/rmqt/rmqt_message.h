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

#ifndef INCLUDED_RMQT_MESSAGE
#define INCLUDED_RMQT_MESSAGE

#include <rmqt_fieldvalue.h>
#include <rmqt_properties.h>

#include <bdlb_guid.h>

#include <bsl_cstddef.h>
#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqt {

/// \brief An AMQP content message
///
/// This class represents an AMQP message that can be published to and consumed
/// from a RabbitMQ broker.
///
/// By default all Messages are sent with Mandatory = true to ensure no message
/// can be silently dropped if no queues are bound. Only disable this flag if
/// you understand this will cause silently discarded messages.
///

class Message {
  public:
    Message();
    /// \brief RabbitMQ message constructor. By default, message will have
    ///        persistent delivery-mode.
    /// \param rawData Message raw data
    /// \param messageId Message ID
    /// \param headers Optional message headers
    Message(const bsl::shared_ptr<bsl::vector<uint8_t> >& rawData,
            const bsl::string& messageId = "",
            const bsl::shared_ptr<rmqt::FieldTable>& headers =
                bsl::shared_ptr<rmqt::FieldTable>());

    /// \brief RabbitMQ message constructor. By default, message will have
    ///        persistent delivery-mode.
    /// \param rawData Message raw data
    /// \param properties Message ID
    Message(const bsl::shared_ptr<bsl::vector<uint8_t> >& rawData,
            const rmqt::Properties& properties);

    /// \brief RabbitMQ message constructor. By default, message will have
    ///        persistent delivery-mode.
    /// \param rawData Message raw data
    /// \param properties Message ID
    Message(const bsl::shared_ptr<const bsl::vector<uint8_t> >& rawData,
            const rmqt::Properties& properties);

    /// \brief Message GUID
    /// \return A globally unique identifier of the message
    const bdlb::Guid& guid() const { return d_guid; }

    /// \brief Message id
    const bsl::string& messageId() const
    {
        return d_properties.messageId.value();
    }

    const bsl::shared_ptr<rmqt::FieldTable>& headers() const
    {
        return d_properties.headers;
    }

    rmqt::Properties& properties() { return d_properties; }

    const rmqt::Properties& properties() const { return d_properties; }

    /// \brief Message payload
    const uint8_t* payload() const
    {
        return d_message ? d_message->data() : NULL;
    }

    /// \brief Message payload size
    bsl::size_t payloadSize() const
    {
        return d_message ? d_message->size() : 0;
    }

    /// \brief Update delivery-mode(Persistent or Non-persistent). Default
    ///        delivery-mode is Persistent for rmqt::Message. Persistent
    ///        messages will be logged to disk, if they are delivered to
    ///        durable queues.
    void updateDeliveryMode(const rmqt::DeliveryMode::Value& deliveryMode)
    {
        d_properties.deliveryMode = deliveryMode;
    }

    /// \brief Update update-priority. Default no priority
    void updateMessagePriority(const bsl::uint8_t& priority)
    {
        d_properties.priority = priority;
    }

    /// \brief Return true, if delivery-mode is persistent for the message
    bool isPersistent() const
    {
        return d_properties.deliveryMode.value_or(
                   rmqt::DeliveryMode::NON_PERSISTENT) ==
               rmqt::DeliveryMode::PERSISTENT;
    }

  private:
    bdlb::Guid d_guid;
    bsl::shared_ptr<const bsl::vector<uint8_t> > d_message;
    Properties d_properties;
};

bsl::ostream& operator<<(bsl::ostream& os, const rmqt::Message& message);
bool operator==(const rmqt::Message& lhs, const rmqt::Message& rhs);
bool operator!=(const rmqt::Message& lhs, const rmqt::Message& rhs);

} // namespace rmqt
} // namespace BloombergLP

#endif
