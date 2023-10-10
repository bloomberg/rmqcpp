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

// rmqamqpt_contentbody.h
#ifndef INCLUDED_RMQAMQPT_CONTENTBODY
#define INCLUDED_RMQAMQPT_CONTENTBODY

#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>
#include <bsl_vector.h>

#include <bsl_cstdint.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Represents, and generates content body frames on request

class ContentBody {
  public:
    ContentBody();

    ContentBody(const uint8_t* data, bsl::size_t dataLength);

    bsl::size_t dataLength() const { return d_data.size(); }

    const bsl::vector<uint8_t>& data() const { return d_data; }

    static void decode(ContentBody* contentBody,
                       const uint8_t* data,
                       bsl::size_t dataLength);
    static void encode(Writer& output, const ContentBody& contentBody);

  private:
    bsl::vector<uint8_t> d_data;
};

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
