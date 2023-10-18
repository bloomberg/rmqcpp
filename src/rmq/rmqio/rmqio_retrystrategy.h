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

#ifndef INCLUDED_RMQIO_RETRYSTRATEGY
#define INCLUDED_RMQIO_RETRYSTRATEGY

#include <bsls_timeinterval.h>

#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqio {

class RetryStrategy {
  public:
    /// Should be called when a retry is attempted
    virtual void attempt() = 0;

    /// Can be called when an attempt has been successful
    virtual void success() = 0;

    /// \return a relative time period for when to make the next attempt
    virtual bsls::TimeInterval getNextRetryInterval() = 0;

    /// Should print out the current state of the strategy
    virtual bsl::ostream& print(bsl::ostream& os) const = 0;

    virtual ~RetryStrategy() {}
};

bsl::ostream& operator<<(bsl::ostream& os, const RetryStrategy& rs);

} // namespace rmqio
} // namespace BloombergLP

#endif
