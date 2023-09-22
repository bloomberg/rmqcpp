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

#ifndef INCLUDED_RMQT_MUTUALSECURITYPARAMETERS
#define INCLUDED_RMQT_MUTUALSECURITYPARAMETERS

#include <rmqt_securityparameters.h>

#include <bsl_string.h>
#include <bsls_keyword.h>

namespace BloombergLP {
namespace rmqt {

/// \brief Mutual TLS Parameters
///

class MutualSecurityParameters : public SecurityParameters {
  private:
    bsl::string d_certificateAuthorityPath;
    bsl::string d_clientCertificatePath;
    bsl::string d_clientKeyPath;

  public:
    MutualSecurityParameters(const bsl::string& caPath,
                             const bsl::string& clientCertPath,
                             const bsl::string& clientKeyPath);

    SecurityParameters::Verification verification() const BSLS_KEYWORD_OVERRIDE;

    SecurityParameters::Method method() const BSLS_KEYWORD_OVERRIDE;

    bsl::string clientCertificatePath() const BSLS_KEYWORD_OVERRIDE;

    bsl::string clientKeyPath() const BSLS_KEYWORD_OVERRIDE;
};
} // namespace rmqt
} // namespace BloombergLP

#endif
