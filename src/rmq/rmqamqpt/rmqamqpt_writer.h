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

#ifndef INCLUDED_RMQAMQPT_WRITER
#define INCLUDED_RMQAMQPT_WRITER

#include <bsl_cstdint.h>
#include <bsl_sstream.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Represents a wrapper around memcpy, holding a pointer to a
/// dynamically allocating container and keeping track of offsets across
/// multiple write calls

class Writer {
  public:
    Writer(bsl::vector<uint8_t>* storage)
    : d_currOffset(0)
    , d_storage(storage)
    {
    }

    Writer(bsl::vector<uint8_t>* storage, size_t startOffset)
    : d_currOffset(startOffset)
    , d_storage(storage)
    {
    }

    ~Writer() {}

    inline void write(const uint8_t* bytes, size_t count)
    {
        d_storage->resize(d_currOffset + count);
        memcpy(d_storage->data() + d_currOffset, bytes, count);
        d_currOffset += count;
    }

  private:
    // No copies
    Writer(const Writer&) BSLS_KEYWORD_DELETED;
    Writer& operator=(const Writer&) BSLS_KEYWORD_DELETED;

    size_t d_currOffset;
    bsl::vector<uint8_t>* d_storage;
};

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
