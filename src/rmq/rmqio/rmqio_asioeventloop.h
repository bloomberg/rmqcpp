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

#ifndef INCLUDED_RMQIO_ASIOEVENTLOOP
#define INCLUDED_RMQIO_ASIOEVENTLOOP

#include <rmqio_eventloop.h>

#include <boost/asio.hpp>
#include <bslmt_condition.h>
#include <bslmt_mutex.h>
#include <bsls_keyword.h>

#include <bsl_memory.h>
#include <bsl_optional.h>

namespace BloombergLP {
namespace rmqio {
class Resolver;
class TimerFactory;

class AsioEventLoop : public EventLoop {
    boost::asio::io_context d_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        d_workGuard;
    bsl::shared_ptr<rmqio::Resolver> d_resolver;
    bsl::shared_ptr<rmqio::TimerFactory> d_timerFactory;

    bslmt::Mutex d_mutex;
    bslmt::Condition d_condition;
    bool d_exited;

  public:
    // CREATORS
    AsioEventLoop();
    virtual ~AsioEventLoop() BSLS_KEYWORD_OVERRIDE;

    bool waitForEventLoopExit(int64_t waitTimeSec)
        BSLS_KEYWORD_OVERRIDE BSLS_KEYWORD_FINAL;

    boost::asio::io_context& context() { return d_context; }

    bsl::shared_ptr<rmqio::Resolver>
    resolver(bool shuffleConnectionEndpoints) BSLS_KEYWORD_OVERRIDE;
    bsl::shared_ptr<rmqio::TimerFactory> timerFactory() BSLS_KEYWORD_OVERRIDE;

  protected:
    void onThreadStarted() BSLS_KEYWORD_OVERRIDE;
    void postImpl(const Item& item) BSLS_KEYWORD_OVERRIDE;
    void dispatchImpl(const Item& item) BSLS_KEYWORD_OVERRIDE;

  private:
    void removeWorkGuard();

    AsioEventLoop(AsioEventLoop&) BSLS_KEYWORD_DELETED;
    AsioEventLoop& operator=(const AsioEventLoop&) BSLS_KEYWORD_DELETED;
};

} // namespace rmqio
} // namespace BloombergLP

#endif
