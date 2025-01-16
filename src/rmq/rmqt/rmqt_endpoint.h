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

#ifndef INCLUDED_RMQT_ENDPOINT
#define INCLUDED_RMQT_ENDPOINT

#include <bsl_cstdint.h>
#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

class SecurityParameters;

/// \brief Base class for AMQP endpoint

class Endpoint {
  public:
    virtual ~Endpoint(){};
    virtual bsl::string formatAddress() const = 0;
    virtual bsl::string hostname() const      = 0;
    virtual bsl::string vhost() const         = 0;
    virtual bsl::uint16_t port() const        = 0;
    virtual bsl::shared_ptr<rmqt::SecurityParameters>
    securityParameters() const;
};

} // namespace rmqt
} // namespace BloombergLP

#endif
