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

#include <rmqio_watchdog.h>

#include <ball_log.h>
#include <bdlf_bind.h>

namespace BloombergLP {
namespace rmqio {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQIO.WATCHDOG")

bool expired(const bsl::weak_ptr<Task>& ptr) { return !ptr.lock(); }
} // namespace

WatchDog::WatchDog(const bsls::TimeInterval& timeout)
: d_timer()
, d_timeout(timeout)
{
}

WatchDog::~WatchDog()
{
    if (d_timer) {
        d_timer->cancel();
    }
}

void WatchDog::addTask(const bsl::weak_ptr<Task>& task)
{
    BALL_LOG_DEBUG << "Watchdog task added";
    d_tasks.push_back(task);
}

void WatchDog::start(const bsl::shared_ptr<TimerFactory>& timerFactory)
{
    d_timer = timerFactory->createWithCallback(bdlf::BindUtil::bind(
        &WatchDog::timerCallback, weak_from_this(), bdlf::PlaceHolders::_1));
    d_timer->reset(d_timeout);
}

void WatchDog::timerCallback(const bsl::weak_ptr<WatchDog>& watchdog_wp,
                             Timer::InterruptReason reason)
{
    if (reason == Timer::EXPIRE) {
        bsl::shared_ptr<WatchDog> watchdog = watchdog_wp.lock();
        if (watchdog) {
            watchdog->run();
        }
        else {
            BALL_LOG_DEBUG << "WatchDog has been destructed";
        }
    }
    else {
        BALL_LOG_DEBUG << "WatchDog timer cancelled";
    }
}

void WatchDog::run()
{
    BALL_LOG_DEBUG << "WatchDog triggered";
    d_timer->reset(d_timeout);
    d_tasks.remove_if(&expired);
    for (bsl::list<bsl::weak_ptr<Task> >::iterator task_wp = d_tasks.begin();
         task_wp != d_tasks.end();
         ++task_wp) {
        bsl::shared_ptr<Task> task = task_wp->lock();
        if (task) {
            task->run();
        }
        else {
            BALL_LOG_ERROR << "Unexpected expired pointer";
        }
    }
}

} // namespace rmqio
} // namespace BloombergLP
