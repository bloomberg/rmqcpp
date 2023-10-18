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

#include <rmqt_simpleendpoint.h>

namespace BloombergLP {
namespace rmqt {

SimpleEndpoint::SimpleEndpoint(bsl::string_view address,
                               bsl::string_view vhost,
                               bsl::uint16_t port)
: d_address(address)
, d_vhost(vhost)
, d_port(port)
{
}

bsl::string SimpleEndpoint::formatAddress() const
{
    return protocol() + "://" + d_address + ":" + bsl::to_string(d_port) + "/" +
           d_vhost;
}

bsl::string SimpleEndpoint::hostname() const { return d_address; }

bsl::string SimpleEndpoint::vhost() const { return d_vhost; }
bsl::uint16_t SimpleEndpoint::port() const { return d_port; }
bsl::string SimpleEndpoint::protocol() const { return "amqp"; }

} // namespace rmqt
} // namespace BloombergLP
