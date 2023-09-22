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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsls_assert.h>

#include <bsl_sstream.h>
#include <bsl_stdexcept.h>

namespace {
void throwOnAssertion(const char* expression, const char* filename, int line)
{
    if (!expression) {
        expression = "<no description>";
    }
    if (!filename) {
        filename = "<no file>";
    }
    bsl::stringstream ss;
    ss << "BSLS_ASSERT @ " << filename << ":" << line << " [" << expression
       << "]";

    throw bsl::runtime_error(ss.str());
}
} // namespace

int main(int argc, char** argv)
{
    testing::InitGoogleMock(&argc, argv);

    // Required to test BSLS_ASSERT presence, converts to runtime_error
    BloombergLP::bsls::AssertFailureHandlerGuard guard(&throwOnAssertion);

    return RUN_ALL_TESTS();
}
