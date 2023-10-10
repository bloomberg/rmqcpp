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

#ifndef INCLUDED_RMQIO_SERIALIZEDFRAME
#define INCLUDED_RMQIO_SERIALIZEDFRAME

#include <rmqamqpt_frame.h>

#include <bsls_keyword.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqio {

class SerializedFrame {
  public:
    explicit SerializedFrame(const rmqamqpt::Frame& frame);
    SerializedFrame(const bsl::uint8_t* data, bsl::size_t length);

    bsl::size_t frameLength() const { return d_length; }

    const bsl::uint8_t* serialized() const
    {
        return d_length == 0 ? NULL : d_buffer->data();
    }

    bool operator==(const SerializedFrame&) const;
    bool operator!=(const SerializedFrame&) const;

  private:
    void operator=(const SerializedFrame&) BSLS_KEYWORD_DELETED;
    SerializedFrame(const SerializedFrame&) BSLS_KEYWORD_DELETED;

  private:
    bsl::size_t d_length;
    bsl::shared_ptr<bsl::vector<bsl::uint8_t> > d_buffer;
};

} // namespace rmqio
} // namespace BloombergLP

#endif
