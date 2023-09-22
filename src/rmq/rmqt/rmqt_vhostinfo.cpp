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

#include <rmqt_vhostinfo.h>

#include <rmqt_credentials.h>
#include <rmqt_endpoint.h>

namespace BloombergLP {
namespace rmqt {

VHostInfo::VHostInfo(const bsl::shared_ptr<rmqt::Endpoint> endpoint,
                     const bsl::shared_ptr<rmqt::Credentials> credentials)
: d_endpoint(endpoint)
, d_credentials(credentials)
{
}

bsl::shared_ptr<rmqt::Endpoint> VHostInfo::endpoint() const
{
    return d_endpoint;
}

bsl::shared_ptr<rmqt::Credentials> VHostInfo::credentials() const
{
    return d_credentials;
}

} // namespace rmqt
} // namespace BloombergLP