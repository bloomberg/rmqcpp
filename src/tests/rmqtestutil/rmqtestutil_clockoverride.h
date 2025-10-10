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

#ifndef INCLUDED_RMQTESTUTIL_CLOCKOVERRIDE
#define INCLUDED_RMQTESTUTIL_CLOCKOVERRIDE

#include <boost/asio.hpp>
#include <rmqio_asiotimer.h>

//@PURPOSE: Provides ClockOverride class extended from
// rmqio::DefaultClockType
//
//@CLASSES:
//  rmqtestutil::ClockOverride: Steps time in testing to trigger handler waiting
//  on timer

namespace BloombergLP {
namespace rmqtestutil {

class ClockOverride : public rmqio::DefaultClockType {
  public:
    static void step_time(duration t) { d_timeOffset += t; }

    static time_point now()
    {
        return rmqio::DefaultClockType::now() + d_timeOffset;
    }
    static duration to_wait_duration(duration d)
    {
        // This is the secret sauce to ensure that boost::asio keeps calling
        // `now()` and responds to our adjustments via `step_time`
        return d < boost::asio::chrono::milliseconds(1)
                   ? d
                   : boost::asio::chrono::milliseconds(1);
    }

    static duration d_timeOffset;
};
} // namespace rmqtestutil
} // namespace BloombergLP

#endif
