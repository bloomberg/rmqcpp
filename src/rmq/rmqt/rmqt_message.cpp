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

#include <rmqt_message.h>

#include <rmqt_fieldvalue.h>
#include <rmqt_properties.h>

#include <ball_log.h>
#include <bdlb_guidutil.h>

#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQT.MESSAGE")

rmqt::Properties
initialiseProperties(const bsl::string& msgId = "",
                     const bsl::shared_ptr<rmqt::FieldTable>& headers =
                         bsl::shared_ptr<rmqt::FieldTable>())
{
    rmqt::Properties p;
    p.headers      = headers;
    p.messageId    = msgId;
    p.deliveryMode = rmqt::DeliveryMode::PERSISTENT;
    return p;
}

void setMessageId(rmqt::Properties& p, bdlb::Guid& guid)
{
    bsl::string messageId = p.messageId.value_or("");
    if (messageId.empty()) {
        guid        = bdlb::GuidUtil::generate();
        p.messageId = bdlb::GuidUtil::guidToString(guid);
    }
    // bdlb::GuidUtil::guidFromString returns 0 on success and a positive value
    // on failure
    else if (bdlb::GuidUtil::guidFromString(&guid, messageId)) {
        BALL_LOG_TRACE << "messageId wasn't a guid";
        guid = bdlb::GuidUtil::generate();
    }
}

} // namespace

Message::Message()
: d_guid()
, d_message()
, d_properties(initialiseProperties())
{
}

Message::Message(const bsl::shared_ptr<bsl::vector<uint8_t> >& rawData,
                 const bsl::string& messageId,
                 const bsl::shared_ptr<rmqt::FieldTable>& headers)
: d_guid()
, d_message(rawData)
, d_properties(initialiseProperties(messageId, headers))
{
    setMessageId(d_properties, d_guid);
}

Message::Message(const bsl::shared_ptr<const bsl::vector<uint8_t> >& rawData,
                 const rmqt::Properties& properties)
: d_guid()
, d_message(rawData)
, d_properties(properties)
{
    setMessageId(d_properties, d_guid);
}

Message::Message(const bsl::shared_ptr<bsl::vector<uint8_t> >& rawData,
                 const rmqt::Properties& properties)
: d_guid()
, d_message(rawData)
, d_properties(properties)
{
    setMessageId(d_properties, d_guid);
}

bsl::ostream& operator<<(bsl::ostream& os, const rmqt::Message& message)
{
    os << "Message = [ "
       << "guid: " << message.guid()
       << ", payloadSize: " << message.payloadSize()
       << ", messageId: " << message.properties().messageId
       << ", headers: OMITTED ]";
    return os;
}

bool operator==(const rmqt::Message& lhs, const rmqt::Message& rhs)
{
    return (&lhs == &rhs) ||
           (lhs.guid() == rhs.guid() && lhs.properties() == rhs.properties());
}

bool operator!=(const rmqt::Message& lhs, const rmqt::Message& rhs)
{
    return !(lhs == rhs);
}

} // namespace rmqt
} // namespace BloombergLP
