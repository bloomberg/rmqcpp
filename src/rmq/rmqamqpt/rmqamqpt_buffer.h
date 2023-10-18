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

#ifndef INCLUDED_RMQAMQPT_BUFFER
#define INCLUDED_RMQAMQPT_BUFFER

#include <bslmf_assert.h>
#include <bslmf_isbitwisemoveable.h>
#include <bslmf_istriviallycopyable.h>
#include <bsls_assert.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_cstring.h>
#include <bsl_type_traits.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief  Wrap a byte array to ease decoding into structures
///
///  Implements typed-reading out of a byte array including skipping over
///  elements & facilitating bounds checks

class Buffer {
  public:
    typedef uint8_t value_type;
    typedef const value_type* const_pointer;
    typedef bsl::size_t size_type;

    Buffer(const_pointer start, size_type length);

#if BSLS_COMPILERFEATURES_CPLUSPLUS >= 201103L
    Buffer()              = delete;
    Buffer(const Buffer&) = default;
    ~Buffer()             = default;
#endif

    template <typename TYPE>
    TYPE copy();

    Buffer consume(size_type size);
    void skip(size_type size);
    const_pointer ptr() const { return d_data + d_offset; }
    size_type available() const { return d_length - d_offset; }

  private:
    typedef bsl::size_t offset_type;

    const_pointer const d_data;
    const size_type d_length;
    offset_type d_offset;
};

template <typename TYPE>
TYPE Buffer::copy()
{
    BSLMF_ASSERT(bsl::is_trivially_copyable<TYPE>::value);
    BSLMF_ASSERT(bslmf::IsBitwiseMoveable<TYPE>::value);
    BSLS_ASSERT(available() >= sizeof(TYPE));
    TYPE val;
    bsl::memcpy(&val, ptr(), sizeof(TYPE));
    skip(sizeof(TYPE));
    return val;
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
