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

#include <rmqio_eventloop.h>

#include <ball_log.h>
#include <bslmt_threadutil.h>

#include <bdlf_bind.h>
#include <bsl_memory.h>
#include <bsl_stdexcept.h>

namespace BloombergLP {
namespace rmqio {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQIO.EVENTLOOP")
} // namespace

EventLoop::EventLoop()
: d_thread()
, d_started(false)
, d_joined(false)
{
}

void EventLoop::join()
{
    if (d_started && !d_joined) {
        bslmt::ThreadUtil::join(d_thread);
        d_joined = true;
    }
}

EventLoop::~EventLoop()
{
    join();
    d_started = false;
}

void EventLoop::start()
{
    // Start worker thread
    if (d_started) {
        BALL_LOG_ERROR << "EventLoop::start() called when worker thread is "
                          "already running";
        return;
    }
    const int error_code = bslmt::ThreadUtil::create(
        &d_thread, bdlf::BindUtil::bind(&EventLoop::run, this));
    if (error_code) {
        BALL_LOG_ERROR
            << "Couldn't start event loop worker thread. Error code: "
            << error_code;
    }
    else {
        postF<void>(bdlf::BindUtil::bind(&EventLoop::loopStarted, this))
            .blockResult();
        d_started = true;
    }
}

bool EventLoop::isStarted() const { return d_started; }

void EventLoop::run()
{
    bslmt::ThreadUtil::setThreadName("RMQIO.EVENTLOOP");
    BALL_LOG_TRACE << "IO thread started";
    this->onThreadStarted();
    BALL_LOG_TRACE << "IO thread finished";
    // d_running = false;?
}

void EventLoop::loopStarted() { BALL_LOG_DEBUG << "Event Loop Running"; }

} // namespace rmqio
} // namespace BloombergLP
