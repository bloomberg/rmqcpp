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

#ifndef INCLUDED_RMQT_EXCHANGEBINDING
#define INCLUDED_RMQT_EXCHANGEBINDING

#include <rmqt_binding.h>
#include <rmqt_exchange.h>
#include <rmqt_fieldvalue.h>

#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief An AMQP exchange binding
///
/// This class represents a binding between an exchange and a queue to be
/// declared to the RabbitMQ broker.

class ExchangeBinding : public Binding {
  public:
    /// \brief Exchange binding constructor
    ///
    /// \param sourceExchange Source exchange
    /// \param destinationExchange Destination exchange
    /// \param bindingKey Binding key
    /// \param args Optional binding arguments
    ExchangeBinding(const ExchangeHandle& sourceExchange,
                    const ExchangeHandle& destinationExchange,
                    const bsl::string& bindingKey,
                    const rmqt::FieldTable& args = rmqt::FieldTable())
    : Binding(bindingKey, args)
    , d_sourceExchange(sourceExchange)
    , d_destinationExchange(destinationExchange)
    {
    }

    const ExchangeHandle& sourceExchange() const { return d_sourceExchange; }
    const ExchangeHandle& destinationExchange() const
    {
        return d_destinationExchange;
    }

    friend bsl::ostream& operator<<(bsl::ostream& os,
                                    const ExchangeBinding& queue);

  private:
    ExchangeHandle d_sourceExchange;
    ExchangeHandle d_destinationExchange;
};

bsl::ostream& operator<<(bsl::ostream& os, const ExchangeBinding& queue);

} // namespace rmqt
} // namespace BloombergLP
#endif
