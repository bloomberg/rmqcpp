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

#include <rmqt_mutualsecurityparameters.h>

namespace BloombergLP {
namespace rmqt {

MutualSecurityParameters::MutualSecurityParameters(
    const bsl::string& caPath,
    const bsl::string& clientCertPath,
    const bsl::string& clientKeyPath)
: SecurityParameters(caPath)
, d_clientCertificatePath(clientCertPath)
, d_clientKeyPath(clientKeyPath)
{
}

SecurityParameters::Verification MutualSecurityParameters::verification() const
{
    return SecurityParameters::MUTUAL;
}

SecurityParameters::Method MutualSecurityParameters::method() const
{
    return SecurityParameters::TLS_1_2_OR_BETTER;
}

bsl::string MutualSecurityParameters::clientCertificatePath() const
{
    return d_clientCertificatePath;
}

bsl::string MutualSecurityParameters::clientKeyPath() const
{
    return d_clientKeyPath;
}

} // namespace rmqt
} // namespace BloombergLP
