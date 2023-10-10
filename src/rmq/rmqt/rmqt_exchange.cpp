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

#include <rmqt_exchange.h>

#include <rmqt_fieldvalue.h>

#include <bdlpcre_regex.h>

#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {
namespace {
const size_t MAX_EXCHANGE_NAME_LENGTH = 127;
}

const char Exchange::DEFAULT_EXCHANGE[] = "";

Exchange::Exchange(
    const bsl::string& name,
    bool passive /* = false */,
    const rmqt::ExchangeType& exchangeType /* = ExchangeType::DIRECT */,
    bool autoDelete /* = false */,
    bool durable /* = true */,
    bool internal /* = false */,
    const rmqt::FieldTable& args /* = rmqt::FieldTable() */)
: d_name(name)
, d_exchangeType(exchangeType.type())
, d_passive(passive)
, d_autoDelete(autoDelete)
, d_durable(durable)
, d_internal(internal)
, d_args(args)
{
}

const bsl::string& Exchange::name() const { return d_name; }

const bsl::string& Exchange::type() const { return d_exchangeType; }

bool Exchange::passive() const { return d_passive; }

bool Exchange::autoDelete() const { return d_autoDelete; }

bool Exchange::durable() const { return d_durable; }

bool Exchange::internal() const { return d_internal; }

const rmqt::FieldTable& Exchange::arguments() const { return d_args; }

bool Exchange::isDefault() const
{
    return d_name == Exchange::DEFAULT_EXCHANGE &&
           static_cast<rmqt::ExchangeType>(d_exchangeType) ==
               rmqt::ExchangeType::DIRECT &&
           !d_autoDelete && d_durable && !d_internal && d_args.size() == 0;
}

bool ExchangeUtil::validateName(const bsl::string& exchangeName)
{
    bdlpcre::RegEx regex;
    bsl::string errMsg;
    size_t errOff;
    regex.prepare(&errMsg, &errOff, "^[a-zA-Z0-9-_.:]*$");
    return (exchangeName.size() <= MAX_EXCHANGE_NAME_LENGTH) &&
           0 == regex.match(exchangeName.c_str(), exchangeName.length());
}

bool operator==(const Exchange& lhs, const Exchange& rhs)
{
    return (&lhs == &rhs) ||
           (lhs.name() == rhs.name() && lhs.passive() == rhs.passive() &&
            lhs.type() == rhs.type() && lhs.autoDelete() == rhs.autoDelete() &&
            lhs.durable() == rhs.durable() &&
            lhs.internal() == rhs.internal() &&
            lhs.arguments() == rhs.arguments());
}

bool operator!=(const Exchange& lhs, const Exchange& rhs)
{
    return !(lhs == rhs);
}

bsl::ostream& operator<<(bsl::ostream& os, const Exchange& exchange)
{
    os << "Exchange = [ name = '" << exchange.d_name << "' type = '"
       << exchange.d_exchangeType << "' Properties = [";
    if (exchange.d_passive) {
        os << " Passive";
    }
    if (exchange.d_autoDelete) {
        os << " Auto-delete";
    }
    if (exchange.d_durable) {
        os << " Durable";
    }
    if (exchange.d_internal) {
        os << " Internal";
    }
    os << " ] Args = " << exchange.d_args << " ]";

    return os;
}

} // namespace rmqt
} // namespace BloombergLP
