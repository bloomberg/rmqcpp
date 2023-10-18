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

#ifndef INCLUDED_RMQTESTUTIL_TIMEOVERRIDE
#define INCLUDED_RMQTESTUTIL_TIMEOVERRIDE

#include <boost/asio.hpp>

//@PURPOSE: Provides TimeOverride class extended from
// boost::asio::deadline_timer::traits_type
//
//@CLASSES:
//  rmqtestutil::TimeOverride: Steps time in testing to trigger handler waiting
//  on deadline_timer

namespace BloombergLP {
namespace rmqtestutil {

class TimeOverride : public boost::asio::deadline_timer::traits_type {
  public:
    static time_type now()
    {
        return add(boost::asio::deadline_timer::traits_type::now(),
                   d_timeOffset);
    }
    static void step_time(duration_type t) { d_timeOffset += t; }
    static boost::posix_time::time_duration to_posix_duration(duration_type d)
    {
        // This is the secret sauce to ensure that boost::asio keeps calling
        // `now()` and responds to our adjustments via `step_time`
        return d < boost::posix_time::milliseconds(1)
                   ? d
                   : boost::posix_time::milliseconds(1);
    }

    static duration_type d_timeOffset;
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif
