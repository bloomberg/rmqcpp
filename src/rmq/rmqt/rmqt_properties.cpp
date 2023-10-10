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

#include <rmqt_properties.h>

#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bslim_printer.h>

namespace BloombergLP {
namespace rmqt {

namespace {
bool headersEquality(const bsl::shared_ptr<rmqt::FieldTable>& lhs,
                     const bsl::shared_ptr<rmqt::FieldTable>& rhs)
{
    return (lhs == rhs) || ((lhs && rhs) && (*lhs == *rhs));
}
} // namespace

bsl::ostream& Properties::print(bsl::ostream& stream,
                                BSLA_MAYBE_UNUSED int level,
                                BSLA_MAYBE_UNUSED int spacesPerLevel) const
{
    const bsl::string_view deliveryModeValue =
        deliveryMode.value_or(rmqt::DeliveryMode::NON_PERSISTENT) ==
                rmqt::DeliveryMode::NON_PERSISTENT
            ? "non persistent"
            : "persistent";
    bslim::Printer printer(&stream, 0, -1);
    printer.start();
    printer.printAttribute("contentType", contentType);
    printer.printAttribute("contentEncoding", contentEncoding);
    printer.printAttribute("headers", headers);
    printer.printAttribute("deliveryMode", deliveryModeValue);
    printer.printAttribute("priority", priority);
    printer.printAttribute("correlationId", correlationId);
    printer.printAttribute("replyTo", replyTo);
    printer.printAttribute("expiration", expiration);
    printer.printAttribute("messageId", messageId);
    printer.printAttribute("timestamp", timestamp);
    printer.printAttribute("type", type);
    printer.printAttribute("userId", userId);
    printer.printAttribute("appId", appId);
    printer.end();
    return stream;
}

bsl::ostream& operator<<(bsl::ostream& os, const Properties& p)
{
    return p.print(os, 0, -1);
}

bool operator==(const Properties& lhs, const Properties& rhs)
{
#define RMQT_PROPERTY_EQUALS(prop) lhs.prop == rhs.prop

    return (&lhs == &rhs) ||
           (RMQT_PROPERTY_EQUALS(contentType) &&
            RMQT_PROPERTY_EQUALS(contentEncoding) &&
            headersEquality(lhs.headers, rhs.headers) &&
            RMQT_PROPERTY_EQUALS(deliveryMode) &&
            RMQT_PROPERTY_EQUALS(priority) &&
            RMQT_PROPERTY_EQUALS(correlationId) &&
            RMQT_PROPERTY_EQUALS(replyTo) && RMQT_PROPERTY_EQUALS(expiration) &&
            RMQT_PROPERTY_EQUALS(messageId) &&
            RMQT_PROPERTY_EQUALS(timestamp) && RMQT_PROPERTY_EQUALS(type) &&
            RMQT_PROPERTY_EQUALS(userId) && RMQT_PROPERTY_EQUALS(appId));
#undef RMQT_PROPERTY_EQUALS
}

bool operator!=(const Properties& lhs, const Properties& rhs)
{
    return !(lhs == rhs);
}

} // namespace rmqt
} // namespace BloombergLP
