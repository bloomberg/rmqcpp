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

#ifndef INCLUDED_RMQAMQP_CHANNELCONTAINER
#define INCLUDED_RMQAMQP_CHANNELCONTAINER

#include <rmqamqp_channelmap.h>

#include <bsls_keyword.h>

namespace BloombergLP {
namespace rmqamqp {

class ChannelContainer {
  public:
    virtual const ChannelMap& channelMap() const = 0;

    /// Returns a string representing a useful name for tracing this connection.
    /// Exclusively for human-readable debugging purposes
    virtual bsl::string connectionDebugName() const = 0;

    ChannelContainer() {}

    virtual ~ChannelContainer() {}

  private:
    ChannelContainer(const ChannelContainer&) BSLS_KEYWORD_DELETED;
    ChannelContainer& operator=(const ChannelContainer&) BSLS_KEYWORD_DELETED;
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
