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

#ifndef INCLUDED_RMQIO_TIMER
#define INCLUDED_RMQIO_TIMER

#include <bsls_timeinterval.h>

#include <bsls_keyword.h>

#include <bsl_functional.h>
#include <bsl_memory.h>

//@PURPOSE: Abstract Timer interface
//
//@CLASSES:
//  rmqio::TimerFactory: Provides an interface for creating Timers
//
//  rmqio::Timer: Provides an interface for scheduling a cancellable callback
//                to be executed after a given timeout

namespace BloombergLP {
namespace rmqio {

class Timer {
  public:
    enum InterruptReason { EXPIRE, CANCEL };

    typedef bsl::function<void(InterruptReason)> Callback;

    /// Cancels pending timer without invoking callback.
    virtual ~Timer() {}

    /// Cancels pending timer and invokes callback with InterruptReason::Cancel
    /// and schedules a new callback with provided timeout.
    /// It is invalid to call `reset` without calling `start` first.
    virtual void reset(const bsls::TimeInterval& timeout) = 0;

    /// Cancels pending timer and invokes callback with
    /// InterruptReason::Cancel.
    virtual void cancel() = 0;

    /// Set d_callback to something new
    /// This must happen from the event loop thread
    virtual void resetCallback(const Callback& callback) = 0;

    /// Schedules a callback to be invoked after `timeout` as set when
    /// constructed or reset. Callback is invoked with InterruptReason:
    ///    Expire - `timeout` has elapsed
    ///    Cancel - Operation canceled by reset or cancel
    virtual void start(const Callback& callback) = 0;

  protected:
    Timer() {}

  private:
    Timer(Timer&) BSLS_KEYWORD_DELETED;
    Timer& operator=(const Timer&) BSLS_KEYWORD_DELETED;
};

class TimerFactory {
  public:
    virtual ~TimerFactory(){};

    /// Creates a timer and initializes its timeout. The timer should be
    /// started by calling start().
    virtual bsl::shared_ptr<rmqio::Timer>
    createWithTimeout(const bsls::TimeInterval& timeout) = 0;

    /// Creates a timer and initializes its callback. The timer should be
    /// started by calling reset().
    virtual bsl::shared_ptr<rmqio::Timer>
    createWithCallback(const Timer::Callback& callback) = 0;
};

} // namespace rmqio
} // namespace BloombergLP

#endif
