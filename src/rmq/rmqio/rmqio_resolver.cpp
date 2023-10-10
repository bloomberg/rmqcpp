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

// rmqio_resolver.cpp   -*-C++-*-
#include <bsl_ostream.h>
#include <rmqio_resolver.h>

namespace BloombergLP {
namespace rmqio {

namespace {
bsl::string err_to_string(Resolver::Error err)
{
    switch (err) {
        case Resolver::ERROR_RESOLVE:
            return "Failed to Resolve";
        case Resolver::ERROR_CONNECT:
            return "Failed to Connect";
        case Resolver::ERROR_HANDSHAKE:
            return "Failed to Handshake";
    }
    return "Unknown Error";
}
} // namespace

bsl::ostream& operator<<(bsl::ostream& os, Resolver::Error err)
{
    return os << err_to_string(err);
}
} // namespace rmqio
} // namespace BloombergLP
