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

#ifndef INCLUDED_RMQT_CONFIRMRESPONSE
#define INCLUDED_RMQT_CONFIRMRESPONSE

#include <bdlb_nullablevalue.h>
#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief An AMQP publisher confirm response
///
/// Represents a response from the RabbitMQ broker acknowledging (or rejecting)
/// a published message. Response status `ACK` means that the message is
/// accepted by the broker. Status `REJECT` or `RETURN` means that there is a
/// problem and broker has not accepted the message.

class ConfirmResponse {
  public:
    enum Status { ACK, REJECT, RETURN };

    /// \brief ConfirmResponse constructor
    ///
    /// \param status Response status (ack/reject/return)
    explicit ConfirmResponse(const Status status);

    /// \brief ConfirmResponse constructor
    ///
    /// \param code Response code
    /// \param reason Reason for the response
    ConfirmResponse(const uint16_t code, const bsl::string& reason);

    Status status() const { return d_status; }
    bdlb::NullableValue<uint16_t> code() const { return d_code; }
    bdlb::NullableValue<bsl::string> reason() const { return d_reason; }

  private:
    Status d_status;
    bdlb::NullableValue<uint16_t> d_code;
    bdlb::NullableValue<bsl::string> d_reason;
};

bsl::ostream& operator<<(bsl::ostream&, const rmqt::ConfirmResponse&);
bool operator==(const rmqt::ConfirmResponse& lhs,
                const rmqt::ConfirmResponse& rhs);

} // namespace rmqt
} // namespace BloombergLP
#endif
