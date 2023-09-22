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

#include <rmqamqp_contentmaker.h>

#include <rmqt_properties.h>

#include <ball_log.h>
#include <bsl_algorithm.h>

namespace BloombergLP {
namespace rmqamqp {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.CONTENTMAKER")
}

ContentMaker::ContentMaker(const rmqamqpt::ContentHeader& contentHeader)
: d_header(bsl::make_shared<rmqamqpt::ContentHeader>(contentHeader))
, d_body(bsl::make_shared<bsl::vector<uint8_t> >())
, d_remainingContentBytes(contentHeader.bodySize())
{
    BALL_LOG_TRACE << "remaining: " << d_remainingContentBytes;
    d_body->reserve(contentHeader.bodySize());
}

bool ContentMaker::done() const { return d_remainingContentBytes <= 0; }

rmqt::Message ContentMaker::message() const
{
    return rmqt::Message(d_body, d_header->properties().toProperties());
}

ContentMaker::ReturnCode
ContentMaker::appendContentBody(const rmqamqpt::ContentBody& contentBody)
{
    BALL_LOG_TRACE << "remaining: " << d_remainingContentBytes
                   << ", body: " << contentBody.dataLength();
    if (d_remainingContentBytes < contentBody.dataLength()) {
        return ERROR;
    }

    bsl::copy(contentBody.data().begin(),
              contentBody.data().end(),
              bsl::back_inserter(*d_body));

    d_remainingContentBytes -= contentBody.dataLength();
    return done() ? DONE : PARTIAL;
}

} // namespace rmqamqp
} // namespace BloombergLP
