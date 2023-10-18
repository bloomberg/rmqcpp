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

#include <rmqamqpt_buffer.h>

#include <bsls_assert.h>

namespace BloombergLP {
namespace rmqamqpt {

Buffer::Buffer(const_pointer start, size_type length)
: d_data(start)
, d_length(length)
, d_offset(0)
{
    BSLS_ASSERT((start && length > 0) || length == 0);
}

Buffer Buffer::consume(size_type size)
{
    Buffer b(ptr(), size);
    skip(size);
    return b;
}

void Buffer::skip(size_type size)
{
    BSLS_ASSERT(d_offset + size <= d_length);
    d_offset += size;
}

} // namespace rmqamqpt
} // namespace BloombergLP
