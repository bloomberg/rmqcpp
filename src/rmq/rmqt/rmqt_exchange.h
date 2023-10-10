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

#ifndef INCLUDED_RMQT_EXCHANGE
#define INCLUDED_RMQT_EXCHANGE

#include <rmqt_exchangetype.h>
#include <rmqt_fieldvalue.h>

#include <bsl_memory.h>
#include <bsl_ostream.h>
#include <bsl_string.h>

//@PURPOSE: Provide an AMQP exchange.
//
//@CLASSES:
//  rmqt::exchange: a RabbitMQ Exchange API object

namespace BloombergLP {
namespace rmqt {

/// \brief An AMQP Exchange
///
/// Represents an Exchange to be declared on the RabbitMQ Broker.
/// `Exchange` objects are not useful outside of a topology. Use
/// `rmqa::Topology::addExchange` to create these.
class Exchange {
  public:
    static const char DEFAULT_EXCHANGE[];
    // CREATORS
    /// Creates an exchange from a number of properties.
    /// Calling this directly is generally not required. Use
    /// `rmqa::Topology::addExchange`
    /// Create an exchange.
    explicit Exchange(
        const bsl::string& name,
        bool passive                           = false,
        const rmqt::ExchangeType& exchangeType = ExchangeType::DIRECT,
        bool autoDelete                        = false,
        bool durable                           = true,
        bool internal                          = false,
        const rmqt::FieldTable& args           = rmqt::FieldTable());

    bool passive() const;
    const bsl::string& name() const;
    const bsl::string& type() const;
    bool autoDelete() const;
    bool durable() const;
    bool internal() const;
    const rmqt::FieldTable& arguments() const;

    /// Checks whether the exchange properties match those of the 'Default'
    /// AMQP exchange:
    ///  - Blank name
    ///  - DIRECT type
    ///  - Auto-delete: false
    ///  - Durable: true
    ///  - Internal: false
    ///  - Empty arguments field table
    bool isDefault() const;

    friend bsl::ostream& operator<<(bsl::ostream& os, const Exchange& exchange);

  private:
    const bsl::string d_name;
    const bsl::string d_exchangeType;
    const bool d_passive;
    const bool d_autoDelete;
    const bool d_durable;
    const bool d_internal;
    const rmqt::FieldTable d_args;
};

/// \brief Utility methods for `rmqt::Exchange`
class ExchangeUtil {
  public:
    /// Validates an exchange name according to the AMQP 0.9.1 spec
    static bool validateName(const bsl::string& exchangeName);
};

typedef bsl::weak_ptr<Exchange> ExchangeHandle;

bool operator==(const Exchange& lhs, const Exchange& rhs);
bool operator!=(const Exchange& lhs, const Exchange& rhs);
bsl::ostream& operator<<(bsl::ostream& os, const Exchange& exchange);

} // namespace rmqt
} // namespace BloombergLP

#endif // ! INCLUDED_RMQT_EXCHANGE
