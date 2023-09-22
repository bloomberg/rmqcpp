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

#include <rmqa_connectionstring.h>

#include <bsl_string.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace rmqa;
using namespace ::testing;

struct ConnectionStringTestCase {
    bsl::string_view uri;
    bool parseSuccess;
    bsl::string_view scheme;
    bsl::string_view username;
    bsl::string_view password;
    bsl::string_view hostname;
    bsl::string_view port;
    bsl::string_view vhost;
};

class ConnectionStringPTests : public TestWithParam<ConnectionStringTestCase> {
};

TEST_P(ConnectionStringPTests, ParseParts)
{
    const ConnectionStringTestCase& testCase = GetParam();

    bsl::string_view scheme   = "";
    bsl::string_view username = "";
    bsl::string_view password = "";
    bsl::string_view hostname = "";
    bsl::string_view port     = "";
    bsl::string_view vhost    = "";

    // if test status doesn't succeed we don't test value of any fields
    ASSERT_THAT(ConnectionString::parseParts(&scheme,
                                             &username,
                                             &password,
                                             &hostname,
                                             &port,
                                             &vhost,
                                             testCase.uri),
                Eq(testCase.parseSuccess));

    EXPECT_THAT(username, Eq(testCase.username));
    EXPECT_THAT(password, Eq(testCase.password));
    EXPECT_THAT(hostname, Eq(testCase.hostname));
    EXPECT_THAT(port, Eq(testCase.port));
    EXPECT_THAT(vhost, Eq(testCase.vhost));
}

const ConnectionStringTestCase k_CONN_STRING_TESTS[] = {
    {"amqp://adam:password@rabbit1.:5672/my-vhost",
     true,
     "amqp",
     "adam",
     "password",
     "rabbit1.",
     "5672",
     "my-vhost"},
    {"amqp://rabbit1./my-vhost",
     true,
     "amqp",
     "",
     "",
     "rabbit1.",
     "",
     "my-vhost"},
    {"amqp://adam@rabbit1./my-vhost",
     true,
     "amqp",
     "adam",
     "",
     "rabbit1.",
     "",
     "my-vhost"},
    {"amqp://rabbit1", true, "amqp", "", "", "rabbit1", "", ""},
    {"://", false, "", "", "", "", "", ""},
    {"nope://", false, "", "", "", "", "", ""},
    {"amqp://", false, "", "", "", "", "", ""},
    {"amqp://rabbit1.:", false, "", "", "", "", "", ""},
};

// We need to stick to INSTANTIATE_TEST_CASE_P for a while longer
// But we do want to build with -Werror in our CI
#pragma GCC diagnostic warning "-Wdeprecated-declarations"

INSTANTIATE_TEST_CASE_P(ConnectionStringTests,
                        ConnectionStringPTests,
                        testing::ValuesIn(k_CONN_STRING_TESTS));
