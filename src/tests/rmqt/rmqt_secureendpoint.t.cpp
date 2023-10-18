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

#include <gtest/gtest.h>

#include <rmqt_mutualsecurityparameters.h>
#include <rmqt_secureendpoint.h>
#include <rmqt_securityparameters.h>

using namespace BloombergLP;
using namespace rmqt;

TEST(SecureEndpoint, SimpleSecurity)
{
    SecureEndpoint e("test.example.com",
                     "rmq-testing",
                     5671,
                     bsl::make_shared<SecurityParameters>("/path/to/CA.crt"));
    ASSERT_EQ("amqps://test.example.com:5671/rmq-testing", e.formatAddress());
    ASSERT_EQ(e.securityParameters()->certificateAuthorityPath(),
              "/path/to/CA.crt");
    ASSERT_TRUE(e.securityParameters()->clientKeyPath().empty());
    ASSERT_TRUE(e.securityParameters()->clientCertificatePath().empty());
    ASSERT_EQ(e.securityParameters()->verification(),
              SecurityParameters::VERIFY_SERVER);
    ASSERT_EQ(e.securityParameters()->method(),
              SecurityParameters::TLS_1_2_OR_BETTER);
}

TEST(SecureEndpoint, MutualTLS)
{
    SecureEndpoint e(
        "test.example.com",
        "rmq-testing",
        5671,
        bsl::make_shared<MutualSecurityParameters>("/path/to/CA.crt",
                                                   "/path/to/client.crt",
                                                   "/path/to/client-key.pem"));

    ASSERT_EQ("amqps://test.example.com:5671/rmq-testing", e.formatAddress());
    ASSERT_EQ(e.securityParameters()->clientKeyPath(),
              "/path/to/client-key.pem");
    ASSERT_EQ(e.securityParameters()->clientCertificatePath(),
              "/path/to/client.crt");
    ASSERT_EQ(e.securityParameters()->certificateAuthorityPath(),
              "/path/to/CA.crt");
    ASSERT_EQ(e.securityParameters()->verification(),
              SecurityParameters::MUTUAL);
    ASSERT_EQ(e.securityParameters()->method(),
              SecurityParameters::TLS_1_2_OR_BETTER);
}
