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

#ifndef INCLUDED_RMQT_BINDING
#define INCLUDED_RMQT_BINDING

#include <rmqt_fieldvalue.h>

#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief An AMQP binding
///
/// This is a base class for representing either a QueueBinding or an
/// ExchangeBinding as specified by AMQP.

class Binding {
  protected:
    /// \brief Constructor
    /// \param bindingKey AMQP binding key
    /// \param args Optional binding arguments
    explicit Binding(const bsl::string& bindingKey,
                     const rmqt::FieldTable& args = rmqt::FieldTable())
    : d_bindingKey(bindingKey)
    , d_args(args)
    {
    }

  public:
    const bsl::string& bindingKey() const { return d_bindingKey; }
    const rmqt::FieldTable& args() const { return d_args; }

  private:
    bsl::string d_bindingKey;
    rmqt::FieldTable d_args;
};

} // namespace rmqt
} // namespace BloombergLP
#endif
