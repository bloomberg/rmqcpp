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

#include <boost/asio.hpp>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bsls_keyword.h>
#include <bsls_timeinterval.h>

#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqio {

//@PURPOSE: Timer implementation using ASIO deadline_timer
//
//@CLASSES:
//  rmqio::AsioTimerFactory: Provides a factory for creating AsioTimers
//
//  rmqio::AsioTimer: Provides a class for scheduling a cancellable
//  callback to be executed after a given timeout

template <typename TIME        = boost::asio::deadline_timer::time_type,
          typename TIME_TRAITS = boost::asio::time_traits<TIME> >
class basic_AsioTimer
: public Timer,
  public bsl::enable_shared_from_this<basic_AsioTimer<TIME, TIME_TRAITS> > {
  public:
    basic_AsioTimer(boost::asio::io_context& context,
                    const bsls::TimeInterval& timeout);
    basic_AsioTimer(boost::asio::io_context& context,
                    const Timer::Callback& callback);
    virtual ~basic_AsioTimer() BSLS_KEYWORD_OVERRIDE;
    virtual void reset(const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;
    virtual void cancel() BSLS_KEYWORD_OVERRIDE;
    virtual void resetCallback(const Callback& callback) BSLS_KEYWORD_OVERRIDE;
    virtual void start(const Timer::Callback& callback) BSLS_KEYWORD_OVERRIDE;

  private:
    basic_AsioTimer(basic_AsioTimer&) BSLS_KEYWORD_DELETED;
    basic_AsioTimer& operator=(const basic_AsioTimer&) BSLS_KEYWORD_DELETED;

    static void
    handler_internal(bsl::weak_ptr<basic_AsioTimer<TIME, TIME_TRAITS> > timer,
                     const Timer::Callback callback,
                     const boost::system::error_code& error);
    void handler(const Timer::Callback& callback,
                 const boost::system::error_code& error);
    void startTimer();

    boost::asio::basic_deadline_timer<TIME, TIME_TRAITS> d_timer;
    Timer::Callback d_callback;
    bsls::TimeInterval d_timeout;
    BALL_LOG_SET_CLASS_CATEGORY("RMQIO.ASIOTIMER");
};

typedef basic_AsioTimer<> AsioTimer;

template <typename TIME        = boost::asio::deadline_timer::time_type,
          typename TIME_TRAITS = boost::asio::time_traits<TIME> >
class basic_AsioTimerFactory : public TimerFactory {
  public:
    basic_AsioTimerFactory(rmqio::AsioEventLoop& eventLoop);
    virtual ~basic_AsioTimerFactory() BSLS_KEYWORD_OVERRIDE {}

    virtual bsl::shared_ptr<rmqio::Timer>
    createWithTimeout(const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;

    virtual bsl::shared_ptr<rmqio::Timer>
    createWithCallback(const Timer::Callback& callback) BSLS_KEYWORD_OVERRIDE;

  private:
    rmqio::AsioEventLoop& d_eventLoop;
    BALL_LOG_SET_CLASS_CATEGORY("RMQIO.ASIOTIMERFACTORY");
};

typedef basic_AsioTimerFactory<> AsioTimerFactory;

template <typename T, typename TT>
basic_AsioTimer<T, TT>::basic_AsioTimer(boost::asio::io_context& io_context,
                                        const bsls::TimeInterval& timeout)
: Timer()
, d_timer(io_context)
, d_callback()
, d_timeout(timeout)
{
}

template <typename T, typename TT>
basic_AsioTimer<T, TT>::basic_AsioTimer(boost::asio::io_context& io_context,
                                        const Timer::Callback& callback)
: Timer()
, d_timer(io_context)
, d_callback(callback)
, d_timeout()
{
}

template <typename T, typename TT>
basic_AsioTimer<T, TT>::~basic_AsioTimer()
{
}

template <typename T, typename TT>
void basic_AsioTimer<T, TT>::reset(const bsls::TimeInterval& timeout)
{
    if (!d_callback) {
        BALL_LOG_ERROR << "reset() called before start()";
        return;
    }
    d_timeout = timeout;
    startTimer();
}

template <typename T, typename TT>
void basic_AsioTimer<T, TT>::cancel()
{
    d_timer.cancel();
}

template <typename T, typename TT>
void basic_AsioTimer<T, TT>::resetCallback(const Callback& callback)
{
    d_callback = callback;
}

template <typename T, typename TT>
void basic_AsioTimer<T, TT>::start(const Timer::Callback& callback)
{
    d_callback = callback;
    startTimer();
}

template <typename T, typename TT>
void basic_AsioTimer<T, TT>::handler_internal(
    bsl::weak_ptr<basic_AsioTimer<T, TT> > timer,
    const Timer::Callback callback,
    const boost::system::error_code& error)
{
    bsl::shared_ptr<basic_AsioTimer<T, TT> > t = timer.lock();
    if (t) {
        t->handler(callback, error);
    }
}

template <typename T, typename TT>
void basic_AsioTimer<T, TT>::handler(const Timer::Callback& callback,
                                     const boost::system::error_code& error)
{
    if (!error && d_timer.expires_at() <= TT::now()) {
        callback(Timer::EXPIRE);
    }
    else {
        if (error != boost::asio::error::operation_aborted) {
            BALL_LOG_ERROR << "Unexpected error code: " << error;
        }
        // Cancelled
        callback(Timer::CANCEL);
    }
}

template <typename T, typename TT>
void basic_AsioTimer<T, TT>::startTimer()
{
    d_timer.expires_from_now(
        boost::posix_time::milliseconds(d_timeout.totalMilliseconds()));
    d_timer.async_wait(
        bdlf::BindUtil::bind(&basic_AsioTimer<T, TT>::handler_internal,
                             this->weak_from_this(),
                             d_callback,
                             bdlf::PlaceHolders::_1));
}

template <typename T, typename TT>
basic_AsioTimerFactory<T, TT>::basic_AsioTimerFactory(
    rmqio::AsioEventLoop& eventLoop)
: d_eventLoop(eventLoop)
{
}

template <typename T, typename TT>
bsl::shared_ptr<rmqio::Timer> basic_AsioTimerFactory<T, TT>::createWithTimeout(
    const bsls::TimeInterval& timeout)
{
    return bsl::make_shared<rmqio::basic_AsioTimer<T, TT> >(
        bsl::ref(d_eventLoop.context()), timeout);
}

template <typename T, typename TT>
bsl::shared_ptr<rmqio::Timer> basic_AsioTimerFactory<T, TT>::createWithCallback(
    const Timer::Callback& callback)
{
    return bsl::make_shared<rmqio::basic_AsioTimer<T, TT> >(
        bsl::ref(d_eventLoop.context()), callback);
}

} // namespace rmqio
} // namespace BloombergLP

#endif
