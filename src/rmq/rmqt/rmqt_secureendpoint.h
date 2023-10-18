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

#ifndef INCLUDED_RMQT_SECUREENDPOINT
#define INCLUDED_RMQT_SECUREENDPOINT

#include <rmqt_securityparameters.h>
#include <rmqt_simpleendpoint.h>

#include <bsl_cstdint.h>
#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsls_keyword.h>

namespace BloombergLP {
namespace rmqt {

/// \brief Base class for AMQP endpoint

class SecureEndpoint : public SimpleEndpoint {
  private:
    bsl::shared_ptr<SecurityParameters> d_params;

  public:
    SecureEndpoint(const bsl::string& address,
                   const bsl::string& vhost,
                   bsl::uint16_t port,
                   const bsl::shared_ptr<SecurityParameters>& params);
    ~SecureEndpoint();

    bsl::shared_ptr<SecurityParameters>
    securityParameters() const BSLS_KEYWORD_OVERRIDE;

  protected:
    bsl::string protocol() const BSLS_KEYWORD_OVERRIDE;
};

} // namespace rmqt
} // namespace BloombergLP

#endif
