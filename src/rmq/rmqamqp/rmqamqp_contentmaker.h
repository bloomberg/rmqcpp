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

// rmqamqp_contentmaker.h
#ifndef INCLUDED_RMQAMQP_CONTENTMAKER
#define INCLUDED_RMQAMQP_CONTENTMAKER

#include <rmqamqpt_contentbody.h>
#include <rmqamqpt_contentheader.h>

#include <rmqt_message.h>

#include <bsl_memory.h>
#include <bsl_vector.h>

//@PURPOSE: Create content (header + body) Frames
//
//@CLASSES:
//  rmqamqp::ContentMaker: Generates content header and body frames on request

namespace BloombergLP {
namespace rmqamqp {

class ContentMaker {
  public:
    enum ReturnCode { DONE, PARTIAL, ERROR };

    explicit ContentMaker(const rmqamqpt::ContentHeader& contentHeader);

    rmqt::Message message() const;

    bool done() const;

    ReturnCode appendContentBody(const rmqamqpt::ContentBody& contentBody);

  private:
    bsl::shared_ptr<rmqamqpt::ContentHeader> d_header;
    bsl::shared_ptr<bsl::vector<uint8_t> > d_body;
    uint64_t d_remainingContentBytes;
}; // class ContentMaker

} // namespace rmqamqp
} // namespace BloombergLP

#endif
