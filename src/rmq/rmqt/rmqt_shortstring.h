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

#ifndef INCLUDED_RMQT_SHORTSTRING
#define INCLUDED_RMQT_SHORTSTRING

#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief Represents an AMQP Short String. Encoding truncates to 255 bytes
///
/// ShortString is used to hold a string which can only be 255 charaters long
/// AMQP has two string types, differentiated by the length of the size field
/// sent before the string.
/// If a `rmqt::ShortString` is longer than 255 characters, it is truncated when
/// sent to the broker

class ShortString : public bsl::string {
  public:
    explicit ShortString(const bsl::string& str = bsl::string());
};

} // namespace rmqt
} // namespace BloombergLP

#endif
