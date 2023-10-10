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

#ifndef INCLUDED_RMQAMQP_METRICS
#define INCLUDED_RMQAMQP_METRICS

//@PURPOSE: Hold information required for publishing metrics
//
//@CLASSES:
//  rmqamqp::Metrics: Store constants/methods useful for publishing metrics
//

namespace BloombergLP {
namespace rmqamqp {

class Metrics {
  public:
    static const char* NAMESPACE;
    static const char* VHOST_TAG;
    static const char* CHANNELTYPE_TAG;
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
