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

#include <rmqio_asioeventloop.h>
#include <rmqio_asioresolver.h>
#include <rmqio_asiotimer.h>

#include <ball_log.h>
#include <boost/asio.hpp>
#include <bslmt_lockguard.h>

#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_stdexcept.h>

namespace BloombergLP {
namespace rmqio {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQIO.ASIOEVENTLOOP")
} // namespace

AsioEventLoop::AsioEventLoop()
: EventLoop()
, d_context()
, d_workGuard(boost::asio::make_work_guard(d_context))
, d_resolver()
, d_timerFactory()
, d_mutex()
, d_condition()
, d_exited(false)
{
}

AsioEventLoop::~AsioEventLoop()
{
    const int64_t SHUTDOWN_WAIT_TIME_SEC = 60;

    bool result = true;
    if (isStarted()) {
        result = waitForEventLoopExit(SHUTDOWN_WAIT_TIME_SEC);
        if (!result) {
            BALL_LOG_FATAL
                << "Event loop didn't stop after " << SHUTDOWN_WAIT_TIME_SEC
                << " seconds. Likely there are outstanding objects "
                   "which should be destructed before ~AsioEventLoop.";
        }
    }

    join();

    if (!result) {
        // If we did wait a while for an exit - let's log that we finished
        BALL_LOG_INFO << "Event loop exited";
    }
}

bool AsioEventLoop::waitForEventLoopExit(int64_t waitTimeSec)
{
    BALL_LOG_TRACE << "Beginning to wait " << waitTimeSec
                   << " seconds for event loop exit";
    bsls::TimeInterval timeoutTime = bsls::SystemTime::nowRealtimeClock();
    timeoutTime.addSeconds(waitTimeSec);

    bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
    if (d_exited) {
        return true;
    }

    // Ensure we aren't the only ones keeping outstanding DNS requests alive
    d_resolver.reset();

    removeWorkGuard();

    while (!d_exited) {
        if (bslmt::Condition::e_TIMED_OUT ==
            d_condition.timedWait(&d_mutex, timeoutTime)) {
            return false;
        }
    }

    BALL_LOG_TRACE << "Stopped waiting for event loop exit";

    return true;
}

void AsioEventLoop::removeWorkGuard() { d_workGuard.reset(); }

void AsioEventLoop::onThreadStarted()
{
    BALL_LOG_TRACE << "asio context.run";
    d_context.run();

    bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
    d_exited = true;
    d_condition.broadcast();
}

void AsioEventLoop::postImpl(const Item& item) { boost::asio::post(d_context, item); }
void AsioEventLoop::dispatchImpl(const Item& item) { boost::asio::dispatch(d_context, item); }

bsl::shared_ptr<rmqio::Resolver>
AsioEventLoop::resolver(bool shuffleConnectionEndpoints)
{
    if (!d_resolver) {
        d_resolver =
            AsioResolver::create(bsl::ref(*this), shuffleConnectionEndpoints);
    }
    return d_resolver;
}

bsl::shared_ptr<rmqio::TimerFactory> AsioEventLoop::timerFactory()
{
    if (!d_timerFactory) {
        d_timerFactory = bsl::make_shared<AsioTimerFactory>(bsl::ref(*this));
    }
    return d_timerFactory;
}
} // namespace rmqio
} // namespace BloombergLP
