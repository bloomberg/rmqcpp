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

#include <rmqt_securityparameters.h>

#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

SecurityParameters::~SecurityParameters() {}

SecurityParameters::SecurityParameters(const bsl::string& caPath)
: d_certificateAuthorityPath(caPath)
{
}

SecurityParameters::Verification SecurityParameters::verification() const
{
    return SecurityParameters::VERIFY_SERVER;
}

SecurityParameters::Method SecurityParameters::method() const
{
    return SecurityParameters::TLS_1_2_OR_BETTER;
}

bsl::string SecurityParameters::certificateAuthorityPath() const
{
    return d_certificateAuthorityPath;
}

bsl::string SecurityParameters::clientCertificatePath() const { return ""; }
bsl::string SecurityParameters::clientKeyPath() const { return ""; }

bsl::ostream& operator<<(bsl::ostream& os, const SecurityParameters& params)
{
    const char* verify_msg = "SERVER";
    switch (params.verification()) {
        case SecurityParameters::VERIFY_SERVER:
            // default
            break;
        case SecurityParameters::MUTUAL:
            verify_msg = "MUTUAL";
            break;
    }

    const char* method_msg = "TLS 1.2 or better";
    switch (params.method()) {
        case SecurityParameters::TLS_1_2_OR_BETTER:
            // default
            break;
    }

    os << "Method: " << method_msg << ", Verify: " << verify_msg
       << ", caPath: " << params.certificateAuthorityPath();

    bsl::string clientCertPath = params.clientCertificatePath();
    if (!clientCertPath.empty()) {
        os << ", clientCertPath: " << clientCertPath
           << ", clientKeyPath: " << params.clientKeyPath();
    }

    return os;
}

} // namespace rmqt
} // namespace BloombergLP
