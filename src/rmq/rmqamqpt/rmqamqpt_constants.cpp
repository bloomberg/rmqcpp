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

#include <rmqamqpt_constants.h>

namespace BloombergLP {
namespace rmqamqpt {

// Convert a compile flag into a const char*
#define STRINGIFY2(a) #a
#define STRINGIFY(a) STRINGIFY2(a)

#ifdef BUILDID
const char* Constants::VERSION = STRINGIFY(BUILDID);
#else
const char* Constants::VERSION = "unknown";
#endif

#undef STRINGIFY2
#undef STRINGIFY

const uint8_t Constants::PROTOCOL_HEADER[] =
    {'A', 'M', 'Q', 'P', 0x00, 0x00, 0x09, 0x01};

const bsl::size_t Constants::PROTOCOL_HEADER_LENGTH = 8;
const bsl::uint8_t Constants::FRAME_END             = 0xCE;
const char* Constants::AUTHENTICATION_MECHANISM     = "PLAIN";
const char* Constants::LOCALE                       = "en_US";
const char* Constants::PLATFORM                     = "C++03";
const char* Constants::PRODUCT = "rmqcpp C++ Client Library";

const bsl::size_t Constants::FLOAT_OCTETS   = 4;
const bsl::size_t Constants::DOUBLE_OCTETS  = 8;
const bsl::size_t Constants::DECIMAL_OCTETS = 5;

} // namespace rmqamqpt
} // namespace BloombergLP
