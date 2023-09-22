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

#include <rmqt_secureendpoint.h>

namespace BloombergLP {
namespace rmqt {

SecureEndpoint::SecureEndpoint(
    const bsl::string& address,
    const bsl::string& vhost,
    bsl::uint16_t port,
    const bsl::shared_ptr<SecurityParameters>& params)
: SimpleEndpoint(address, vhost, port)
, d_params(params)
{
}

SecureEndpoint::~SecureEndpoint() {}

bsl::shared_ptr<SecurityParameters> SecureEndpoint::securityParameters() const
{
    return d_params;
}

bsl::string SecureEndpoint::protocol() const { return "amqps"; }

} // namespace rmqt
} // namespace BloombergLP
