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

#include <rmqamqpt_contentbody.h>

#include <bsl_cstdint.h>

namespace BloombergLP {
namespace rmqamqpt {

ContentBody::ContentBody()
: d_data()
{
}

ContentBody::ContentBody(const uint8_t* data, bsl::size_t dataLength)
: d_data(data, data + dataLength)
{
}

void ContentBody::decode(ContentBody* contentBody,
                         const uint8_t* data,
                         bsl::size_t dataLength)
{
    contentBody->d_data = bsl::vector<uint8_t>(data, data + dataLength);
}

void ContentBody::encode(Writer& output, const ContentBody& contentBody)
{
    output.write(contentBody.data().data(), contentBody.dataLength());
}

} // namespace rmqamqpt
} // namespace BloombergLP
