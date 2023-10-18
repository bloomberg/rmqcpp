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

#ifndef INCLUDED_RMQIO_EVENTLOOP
#define INCLUDED_RMQIO_EVENTLOOP

#include <rmqt_future.h>
#include <rmqt_result.h>

#include <bdlf_bind.h>
#include <bdlf_placeholder.h>
#include <bslmt_threadutil.h>
#include <bsls_keyword.h>

#include <bsl_functional.h>
#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqio {
class Resolver;
class TimerFactory;

class EventLoop {
    bslmt::ThreadUtil::Handle d_thread;
    bool d_started;
    bool d_joined;

  public:
    typedef bsl::function<void()> Item;

    // CREATORS
    EventLoop();
    virtual ~EventLoop();

    /// This is faster if you don't need a Future to block on when the work is
    /// done
    void post(const Item& item);

    /// PostF methods add work to the event loop and return a Future of the
    /// return type of the function which resolves when the function is
    /// executed
    template <typename T>
    rmqt::Future<T> postF(const bsl::function<T()>& tProducer);

    void dispatch(const Item& item);

    /// Blocks until the event loop is running and has processed it's first
    /// event
    void start();

    virtual bool isStarted() const;

    virtual bsl::shared_ptr<Resolver>
    resolver(bool shuffleConnectionEndpoints)            = 0;
    virtual bsl::shared_ptr<TimerFactory> timerFactory() = 0;

    /// Attempt to soft-close event loop, waiting up to `waitTimeSec` for
    /// closure
    virtual bool waitForEventLoopExit(int64_t waitTimeSec) = 0;

  protected:
    virtual void onThreadStarted()              = 0;
    virtual void postImpl(const Item& item)     = 0;
    virtual void dispatchImpl(const Item& item) = 0;
    void join();

  private:
    EventLoop(EventLoop&) BSLS_KEYWORD_DELETED;
    EventLoop& operator=(const EventLoop&) BSLS_KEYWORD_DELETED;

    void loopStarted();
    void run();
};

inline void EventLoop::post(const Item& item) { this->postImpl(item); }
inline void EventLoop::dispatch(const Item& item) { this->dispatchImpl(item); }

template <typename T>
rmqt::Future<T> EventLoop::postF(const bsl::function<T()>& tProducer)
{
    typename rmqt::Future<T>::Pair futurePair(rmqt::Future<T>::make());

    post(bdlf::BindUtil::bind(&rmqt::FutureUtil::processResult<T>,
                              futurePair.first,
                              rmqt::FutureUtil::resultWrapper<T>(tProducer)));
    return futurePair.second;
}

} // namespace rmqio
} // namespace BloombergLP

#endif
