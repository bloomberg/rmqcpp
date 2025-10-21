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

#ifndef INCLUDED_RMQIO_ASIOTIMER
#define INCLUDED_RMQIO_ASIOTIMER

#include <rmqio_asioeventloop.h>
#include <rmqio_timer.h>

#include <asio.hpp>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bsls_keyword.h>
#include <bsls_timeinterval.h>

#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqio {

//@PURPOSE: Timer implementation using ASIO steady_timer
//
//@CLASSES:
//  rmqio::AsioTimerFactory: Provides a factory for creating AsioTimers
//
//  rmqio::AsioTimer: Provides a class for scheduling a cancellable
//  callback to be executed after a given timeout

class AsioTimer
: public Timer,
  public bsl::enable_shared_from_this<AsioTimer> {
  public:
    AsioTimer(asio::io_context& context, const bsls::TimeInterval& timeout);
    AsioTimer(asio::io_context& context, const Timer::Callback& callback);
    virtual ~AsioTimer() BSLS_KEYWORD_OVERRIDE;
    virtual void reset(const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;
    virtual void cancel() BSLS_KEYWORD_OVERRIDE;
    virtual void resetCallback(const Callback& callback) BSLS_KEYWORD_OVERRIDE;
    virtual void start(const Timer::Callback& callback) BSLS_KEYWORD_OVERRIDE;

  private:
    AsioTimer(AsioTimer&) BSLS_KEYWORD_DELETED;
    AsioTimer& operator=(const AsioTimer&) BSLS_KEYWORD_DELETED;

    static void handler_internal(bsl::weak_ptr<AsioTimer> timer,
                                 const Timer::Callback callback,
                                 const asio::error_code& error);
    void handler(const Timer::Callback& callback, const asio::error_code& error);
    void startTimer();

    asio::steady_timer d_timer;
    Timer::Callback d_callback;
    bsls::TimeInterval d_timeout;
    BALL_LOG_SET_CLASS_CATEGORY("RMQIO.ASIOTIMER");
};

class AsioTimerFactory : public TimerFactory {
  public:
    AsioTimerFactory(rmqio::AsioEventLoop& eventLoop);
    virtual ~AsioTimerFactory() BSLS_KEYWORD_OVERRIDE {}

    virtual bsl::shared_ptr<rmqio::Timer>
    createWithTimeout(const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;

    virtual bsl::shared_ptr<rmqio::Timer>
    createWithCallback(const Timer::Callback& callback) BSLS_KEYWORD_OVERRIDE;

  private:
    rmqio::AsioEventLoop& d_eventLoop;
    BALL_LOG_SET_CLASS_CATEGORY("RMQIO.ASIOTIMERFACTORY");
};

//================
// AsioTimer impl
//================
inline AsioTimer::AsioTimer(asio::io_context& io_context,
                            const bsls::TimeInterval& timeout)
: Timer()
, d_timer(io_context)
, d_callback()
, d_timeout(timeout)
{
}

inline AsioTimer::AsioTimer(asio::io_context& io_context,
                            const Timer::Callback& callback)
: Timer()
, d_timer(io_context)
, d_callback(callback)
, d_timeout()
{
}

inline AsioTimer::~AsioTimer() {}

inline void AsioTimer::reset(const bsls::TimeInterval& timeout)
{
    if (!d_callback) {
        BALL_LOG_ERROR << "reset() called before start()";
        return;
    }
    d_timeout = timeout;
    startTimer();
}

inline void AsioTimer::cancel() { d_timer.cancel(); }

inline void AsioTimer::resetCallback(const Callback& callback)
{
    d_callback = callback;
}

inline void AsioTimer::start(const Timer::Callback& callback)
{
    d_callback = callback;
    startTimer();
}

inline void AsioTimer::handler_internal(bsl::weak_ptr<AsioTimer> timer,
                                        const Timer::Callback callback,
                                        const asio::error_code& error)
{
    bsl::shared_ptr<AsioTimer> t = timer.lock();
    if (t) {
        t->handler(callback, error);
    }
}

inline void AsioTimer::handler(const Timer::Callback& callback,
                               const asio::error_code& error)
{
    if (!error) {
        callback(Timer::EXPIRE);
    }
    else {
        if (error != asio::error::operation_aborted) {
            BALL_LOG_ERROR << "Unexpected error code: " << error;
        }
        // Cancelled
        callback(Timer::CANCEL);
    }
}

inline void AsioTimer::startTimer()
{
    using namespace std::chrono;
    d_timer.expires_after(milliseconds(d_timeout.totalMilliseconds()));
    d_timer.async_wait(bdlf::BindUtil::bind(&AsioTimer::handler_internal,
                                            this->weak_from_this(),
                                            d_callback,
                                            bdlf::PlaceHolders::_1));
}

//=========================
// AsioTimerFactory impl
//=========================
inline AsioTimerFactory::AsioTimerFactory(rmqio::AsioEventLoop& eventLoop)
: d_eventLoop(eventLoop)
{
}

inline bsl::shared_ptr<rmqio::Timer>
AsioTimerFactory::createWithTimeout(const bsls::TimeInterval& timeout)
{
    return bsl::make_shared<rmqio::AsioTimer>(bsl::ref(d_eventLoop.context()),
                                              timeout);
}

inline bsl::shared_ptr<rmqio::Timer>
AsioTimerFactory::createWithCallback(const Timer::Callback& callback)
{
    return bsl::make_shared<rmqio::AsioTimer>(bsl::ref(d_eventLoop.context()),
                                              callback);
}

} // namespace rmqio
} // namespace BloombergLP

#endif
