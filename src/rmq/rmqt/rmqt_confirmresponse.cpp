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

#include <rmqt_confirmresponse.h>

#include <bdlb_optionalprinter.h>
#include <bsls_review.h>

#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqt {

ConfirmResponse::ConfirmResponse(const Status status)
: d_status(status)
, d_code()
, d_reason()
{
    // The other constructor is for RETURN responses
    BSLS_REVIEW(status != RETURN);
}

ConfirmResponse::ConfirmResponse(const uint16_t code, const bsl::string& reason)
: d_status(RETURN)
, d_code(bdlb::NullableValue<uint16_t>(code))
, d_reason(bdlb::NullableValue<bsl::string>(reason))
{
}

bool operator==(const rmqt::ConfirmResponse& lhs,
                const rmqt::ConfirmResponse& rhs)
{
    return (&lhs == &rhs) ||
           (lhs.status() == rhs.status() && lhs.code() == rhs.code() &&
            lhs.reason() == rhs.reason());
}

bsl::ostream& operator<<(bsl::ostream& os,
                         const rmqt::ConfirmResponse& confirmResponse)
{
    os << "Confirm response from broker = [ ";
    switch (confirmResponse.status()) {
        case rmqt::ConfirmResponse::RETURN:
            os << "status: RETURNED"
               << " replycode: "
               << bdlb::OptionalPrinterUtil::makePrinter(confirmResponse.code())
               << " reason: "
               << bdlb::OptionalPrinterUtil::makePrinter(
                      confirmResponse.reason());
            break;

        case rmqt::ConfirmResponse::REJECT:
            os << "status: REJECTED";
            break;

        default:
        case rmqt::ConfirmResponse::ACK:
            os << "status: ACCEPTED";
            break;
    }
    os << " ]";
    return os;
}

} // namespace rmqt
} // namespace BloombergLP
