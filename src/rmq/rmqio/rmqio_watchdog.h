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

#ifndef INCLUDED_RMQIO_WATCHDOG
#define INCLUDED_RMQIO_WATCHDOG

#include <rmqio_task.h>
#include <rmqio_timer.h>

#include <bsls_keyword.h>
#include <bsls_timeinterval.h>

#include <bsl_list.h>
#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqio {

//@PURPOSE: Schedule recurring tasks
//
//@CLASSES:
//  rmqio::WatchDog: Provides facility for running recurring tasks

class WatchDog : public bsl::enable_shared_from_this<WatchDog> {
  public:
    explicit WatchDog(const bsls::TimeInterval& timeout);
    ~WatchDog();

    void addTask(const bsl::weak_ptr<Task>& task);

    void start(const bsl::shared_ptr<TimerFactory>& timerFactory);

  private:
    WatchDog(const WatchDog&) BSLS_KEYWORD_DELETED;
    WatchDog& operator=(const WatchDog&) BSLS_KEYWORD_DELETED;

    static void timerCallback(const bsl::weak_ptr<WatchDog>& watchdog_wp,
                              Timer::InterruptReason reason);

    void run();

    bsl::list<bsl::weak_ptr<Task> > d_tasks;
    bsl::shared_ptr<Timer> d_timer;
    bsls::TimeInterval d_timeout;
};

} // namespace rmqio
} // namespace BloombergLP

#endif
