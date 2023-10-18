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

#ifndef INCLUDED_RMQT_FUTURE
#define INCLUDED_RMQT_FUTURE

#include <rmqt_result.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bslmt_condition.h>
#include <bslmt_lockguard.h>
#include <bslmt_mutex.h>
#include <bsls_assert.h>
#include <bsls_systemtime.h>

#include <bsl_exception.h>
#include <bsl_functional.h>
#include <bsl_list.h>
#include <bsl_memory.h>
#include <bsl_utility.h>

//@PURPOSE: An async-style Future/Promise object
//
//@CLASSES:
//  rmqt::Future: Represents an action which may or may not have completed yet.
//
//  rmqt::Future<T>::Impl: The thread-safe control block for Future<T>. These
//                         are shared between copies of `Future` objects.
//
// rmqt::FutureUtil: 'helper' functionality for working with Futures which is
//                    used in multiple places

namespace BloombergLP {
namespace rmqt {

/// \brief An async-style Future/Promise object
///
/// `Future<T>` Represents an action which may or may not have completed yet.
/// `Future<T>::Maker` completes an associated Future object.
///
/// Each `Future` holds shared ownership of a Control Block (Impl). Copying an
/// existing Future always shares the existing control block. Creating a new
/// future through `Future<T>::make` always creates a new standalone control
/// block.
///
/// Future objects are cancellable. Any cancel function passed to
/// `Future<T>::make` is called and then destroyed when a Control Block is
/// destructed.
///
/// One or more `bsl::function` objects may be scheduled to run upon the
/// completion of a Future object via `Future<T>::then` and
/// `Future<T>::thenFuture`. If scheduled, these are executed from within the
/// `Future<T>::Maker` call.
///
/// `Future<T>::then`/`thenFuture` both return a Future object with a new
/// Control Block. The new Control Block shares ownership of the original
/// Future's control block.
///
/// Demonstration:
/// Pair a = Future<T>::make();
/// Future<T> b = a.then([](const rmqt::Result<T>& res){ return res; });
///
///           a: Fut A     b: Fut B
///               |            |
///               V            V
///              CB A <------ CB B
///
/// thenFuture Demonstration:
/// Future<T>::Pair a = Future<T>::make();
/// Future<T> b = a.thenFuture([](const rmqt::Result<T>& res) {
///     Future<T> c = Future<c>::make();
///     return c;
/// });
///
/// Ownership model prior to `a` being resolved:
///        a.2nd: Fut A     b: Fut B
///               |            |
///               V            V
///              CB A <------ CB B
///
/// Ownership after `a` is resolved:
///
///        a.2nd: Fut A    b: Fut B     c: Fut C     d: Fut D
///                |            |           |             |
///                V            V           V             V
///               CB A         CB B        CB C <------- CB D
///                             |                         ^
///                             |                         |
///                             ---------------------------

template <typename T>
class Future;

template <typename T = void>
class Future {
  public:
    /// Used to resolve a Future<T>. Somewhat equivalent to std::promise
    typedef bsl::function<void(const Result<T>& result)> Maker;

    /// A (Promise, Future) pair. Used to create Futures
    typedef bsl::pair<typename Future<T>::Maker, Future<T> > Pair;

    /// \brief Creates a pair of (Promise, Future).
    ///
    /// The Future is used by the 'receiver' to wait on the completed work.
    /// The Promise is used to complete the Future.
    static Pair make();

    /// \brief Create a pair of (Promise, Future) with a cancel function
    ///
    /// See `Future<T>::make()` for info on Promise & Future
    ///
    /// \param cancelFunction is called when no Future exists to listen for the
    /// result:
    ///      This Future and all chained Futures have been destructed.
    static Pair make(const bsl::function<void()>& cancelFunction);

    /// Destructor
    ~Future();

    /// \brief Attempt to fetch the result, do not block.
    ///
    /// `tryResult` will return the Result<T> if the Promise for this future
    /// has been executed.
    /// If the Future has not been resolved, the Result<T> is falsey, and will
    /// have the returnCode `rmqt::ReturnCodes::TIMEOUT`
    Result<T> tryResult();

    /// \brief Fetch the result, and block if it isn't ready yet
    ///
    /// This method will always return the result passed by the Promise.
    Result<T> blockResult();

    /// \brief Fetch the result, waiting up to `absoluteTime` if it isn't
    /// ready
    ///
    /// \param absoluteTime is an absolute point in time e.g. , not a
    ///              relative to now time period.
    /// \return The resolved result, or a timeout Result
    Result<T> timedWaitResult(const bsls::TimeInterval& absoluteTime);

    /// \brief  Fetch the result, waiting up to `relativeTimeout`
    /// period (from now) if it isn't ready
    ///
    /// \param relativeTimeout is a relative time (e.g. 5 seconds), not an
    ///              absolute point in time.
    /// \return The resolved result, or a timeout Result.
    Result<T> waitResult(const bsls::TimeInterval& relativeTimeout);

    /// instantly resolved future
    explicit Future(const Result<T>& result);

    /// \brief Copy construct this Future.
    ///
    /// Both Futures remain valid, and can be used safely from different
    /// threads.
    Future(const Future& future);

    /// \brief Assignment (copy) Future
    ///
    /// Both remaining Futures remain valid, and can be used safely from
    /// different threads.
    Future& operator=(const Future& future);

    template <typename newT>
    Future<newT>
    then(const bsl::function<Result<newT>(const Result<T>&)>& converter);

    template <typename newT>
    Future<newT>
    thenFuture(const bsl::function<Future<newT>(const Result<T>&)>& nextFuture);

  private:
    class Impl;

  private:
#ifdef __xlC__
    // IBM compilers don't feel the above friend declaration is good enough
    // to allow permission to a future of a different T's
    // impl, so this is necessary. But also doesn't sit
    // well with pretty much any other compiler
    template <typename newT>
    friend class typename Future<newT>::Impl;
#else
    // Required for Future<T>::Impl::addChain
    template <typename newT>
    friend class Future;
#endif

    Future();

    bsl::shared_ptr<Impl> d_impl;
}; // class Future

class FutureUtil {

  public:
    /// Construct `B` with a ManagedPtr<A> created from Result<A>.
    /// Errors are propagated
    template <typename A, typename B>
    static Result<B> convertViaManagedPtr(const Result<A>& a);

    template <typename A, typename B>
    static bsl::function<Future<B>(const Result<A>&)> propagateError(
        const bsl::function<Future<B>(const bsl::shared_ptr<A>&)>& fn)
    {
        return bdlf::BindUtil::bind(
            &propagateErrorImplWithItem<A, B>, fn, bdlf::PlaceHolders::_1);
    }

    template <typename T>
    static bsl::function<Future<T>(const Result<void>&)>
    propagateError(const bsl::function<Future<T>()>& fn)
    {

        return bdlf::BindUtil::bind(
            &propagateErrorImpl<T>, fn, bdlf::PlaceHolders::_1);
    }

    template <typename T>
    static bsl::function<rmqt::Result<T>()>
    resultWrapper(const bsl::function<T()>& tProducer)
    {
        return bdlf::BindUtil::bind(&resultWrapperImpl<T>, tProducer);
    }

    template <typename T>
    static void processResult(const typename Future<T>::Maker& maker,
                              const bsl::function<Result<T>()>& resultProducer)
    {
        maker(resultProducer());
    }

    template <typename T>
    static bsl::function<Result<T>(const Result<T>&)>
    makerWrapper(const typename Future<T>::Maker& maker)
    {
        return bdlf::BindUtil::bind(
            &makerWrapperImpl<T>, maker, bdlf::PlaceHolders::_1);
    }

    template <typename T>
    static bsl::function<Future<T>(const Result<Future<T> >&)> unravelFuture()
    {
        bsl::function<Future<T>(const bsl::shared_ptr<Future<T> >&)> convert =
            bdlf::BindUtil::bind(&unravelImpl<T>, bdlf::PlaceHolders::_1);
        return propagateError<Future<T> >(convert);
    }

    template <typename T>
    static Future<T> flatten(Future<Future<T> > futurefuture)
    {
        return futurefuture.template thenFuture<T>(
            FutureUtil::unravelFuture<T>());
    }

  private:
    template <typename T>
    static Future<T> propagateErrorImpl(const bsl::function<Future<T>()>& t,
                                        const Result<void>& r)
    {
        if (!r) {
            typename Future<T>::Pair fail = Future<T>::make();
            fail.first(Result<T>(r.error(), r.returnCode()));
            return fail.second;
        }
        return t();
    }

    template <typename A, typename B>
    static Future<B> propagateErrorImplWithItem(
        const bsl::function<Future<B>(const bsl::shared_ptr<A>&)>& b,
        const Result<A>& a)
    {
        if (!a) {
            typename Future<B>::Pair fail = Future<B>::make();
            fail.first(Result<B>(a.error(), a.returnCode()));
            return fail.second;
        }
        return b(a.value());
    }

    template <typename T>
    static Result<T> makerWrapperImpl(const typename Future<T>::Maker& maker,
                                      const Result<T>& result)
    {
        maker(result);
        return result;
    }

    template <typename T>
    static Future<T> unravelImpl(const bsl::shared_ptr<Future<T> >& t)
    {
        return *t;
    }

    template <typename T>
    static Result<T> resultWrapperImpl(const bsl::function<T()>& tProducer);
};

template <typename T>
bsl::pair<typename Future<T>::Maker, Future<T> > Future<T>::make()
{
    Future<T> f;
    f.d_impl = bsl::make_shared<Impl>(bsl::function<void()>());

    return bsl::make_pair(f.d_impl->generateMaker(), f);
}

template <typename T>
bsl::pair<typename Future<T>::Maker, Future<T> >
Future<T>::make(const bsl::function<void()>& cancelFunction)
{
    Future<T> f;
    f.d_impl = bsl::make_shared<Impl>(cancelFunction);

    return bsl::make_pair(f.d_impl->generateMaker(), f);
}

template <typename T>
Future<T>::~Future()
{
}

template <typename T>
Result<T> Future<T>::tryResult()
{
    return waitResult(bsls::TimeInterval(0));
}

template <typename T>
Result<T> Future<T>::blockResult()
{
    d_impl->blockUntilMade();
    return d_impl->result();
}

template <typename T>
Result<T> Future<T>::waitResult(const bsls::TimeInterval& relativeTimeout)
{
    bsls::TimeInterval timeoutTime = bsls::SystemTime::nowRealtimeClock();
    timeoutTime += relativeTimeout;
    return timedWaitResult(timeoutTime);
}

template <typename T>
Result<T> Future<T>::timedWaitResult(const bsls::TimeInterval& absoluteTime)
{
    if (d_impl->timedWaitUntilMade(absoluteTime)) {
        return d_impl->result();
    }
    return Result<T>("TIMED OUT", TIMEOUT);
}

template <typename T>
Future<T>::Future(const rmqt::Result<T>& result)
: d_impl()
{
    Pair p = make();
    d_impl = p.second.d_impl;
    p.first(result);
}

template <typename T>
Future<T>::Future()
: d_impl()
{
}

template <typename T>
Future<T>::Future(const Future<T>& future)
: d_impl(future.d_impl)
{
}

template <typename T>
Future<T>& Future<T>::operator=(const Future<T>& future)
{
    this->d_impl = future.d_impl;
    return *this;
}

template <typename T>
template <typename newT>
Future<newT>
Future<T>::then(const bsl::function<Result<newT>(const Result<T>&)>& converter)
{
    return d_impl->addChain(converter);
}

template <typename T>
template <typename newT>
Future<newT> Future<T>::thenFuture(
    const bsl::function<Future<newT>(const Result<T>&)>& nextFutureMaker)
{
    return d_impl->addChain(nextFutureMaker);
}

template <typename T>
Result<T> FutureUtil::resultWrapperImpl(const bsl::function<T()>& tProducer)
{
    return rmqt::Result<T>(bsl::make_shared<T>(tProducer()));
}

template <>
Result<void>
FutureUtil::resultWrapperImpl<void>(const bsl::function<void()>& tProducer);

template <typename A, typename B>
Result<B> FutureUtil::convertViaManagedPtr(const Result<A>& a)
{
    if (a) {
        bslma::ManagedPtr<A> am(a.value().managedPtr());
        return Result<B>(bsl::shared_ptr<B>(new B(am)));
    }
    return Result<B>(a.error(), a.returnCode());
}

template <typename T>
class Future<T>::Impl
: public bsl::enable_shared_from_this<typename Future<T>::Impl> {
  private:
    static void made(const bsl::weak_ptr<typename Future<T>::Impl>& weakSelf,
                     const Result<T>& item)
    {
        bsl::shared_ptr<Future<T>::Impl> self = weakSelf.lock();

        if (!self) {
            BALL_LOG_SET_CATEGORY("RMQT.FUTURE.IMPL");
            BALL_LOG_DEBUG << "Resolved " << bsl::string(!item ? "un" : "")
                           << "successful future<" << typeid(T).name()
                           << "> after cancel";
            // This is a safe race condition - e.g. if a user stops caring about
            // a future but the future resolves anyway in the background, we
            // just drop the result.
            return;
        }

        bslmt::LockGuard<bslmt::Mutex> guard(&self->d_mutex);
        self->d_result = item;
        self->d_done   = true;
        self->notifyChain(&guard);
        self->d_condition.broadcast(); // there could be more than one waiter
    }

    template <typename newT>
    void static converter(
        const typename Future<newT>::Maker& maker,
        const bsl::function<Result<newT>(const Result<T>&)>& newTConverter,
        const Result<T>& result)
    {
        maker(newTConverter(result));
    }

    /// \brief Core implementation details of `thenFuture`
    ///
    /// This function handles the data, and ownership passing required for
    /// `thenFuture`.
    ///
    /// Future objects are labelled A, B, C, D as per the diagram in the class
    /// description
    template <typename newT>
    static void futureConverter(
        const bsl::function<Future<newT>(const Result<T>&)>& cGenerator,
        const typename Future<newT>::Maker& bMaker,
        const bsl::weak_ptr<typename Future<newT>::Impl>& wpBImpl,
        const Result<T>& aResult)
    {
        bsl::shared_ptr<typename Future<newT>::Impl> bImpl = wpBImpl.lock();
        if (bImpl) {
            Future<newT> cFut = cGenerator(aResult);

            Future<newT> dFut =
                cFut.then(FutureUtil::makerWrapper<newT>(bMaker));

            bImpl->updateCancel(dFut.d_impl->getLifetimeExtension());
        }
    }

    static void
    extendChainParentLifetime(const bsl::shared_ptr<typename Future<T>::Impl>&)
    {
        // This function exists only as a bind target so that a Future
        // created in .then() can hold onto the parent Future to keep it
        // alive as long as itself.

        // E.g.

        // rmqt::Future<bsl::string> asyncGetMultiplyAsString(int a, int b)
        // {
        //     rmqt::Future<int> x = asyncMultiply(a, b);
        //     rmqt::Future<bsl::string> y =
        //     x.then<bsl::string>(&convertIntToString);
        //
        //     return y;
        // }

        // In the above example, `x` will go out of scope at the end of the
        // function, but it's successor `y` is still alive.

        // Since the type of `y` does not know about `x`, but it needs to
        // extend it's lifetime (call the `x` destructor when `y`
        // destructs), we need to hold a type-erased form of `x`

        // Note: if `y` then goes out of scope, the whole future chain
        // should be destructed
    }

  public:
    Impl(const bsl::function<void()>& cancelFunc)
    : d_mutex()
    , d_condition()
    , d_result("Null")
    , d_chain()
    , d_done(false)
    , d_cancelFunc(cancelFunc)
    {
    }

    ~Impl()
    {
        BALL_LOG_SET_CATEGORY("RMQT.FUTURE.IMPL");
        try {
            bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
            if (!d_done && d_cancelFunc) {
                d_cancelFunc();
            }
        }
        catch (bsl::exception& e) {
            BALL_LOG_ERROR << "Caught exception in future<" << typeid(T).name()
                           << "> dtor: " << e.what();
        }
        catch (...) {
            BALL_LOG_ERROR << "Caught unknown exception in future<"
                           << typeid(T).name() << "> dtor";
        }
    }

    void blockUntilMade()
    {
        bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
        while (!d_done) {
            d_condition.wait(&d_mutex);
        }
    }

    // returns true if there was no timeout
    bool timedWaitUntilMade(const bsls::TimeInterval& absoluteTime)
    {
        bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
        while (!d_done) {
            if (bslmt::Condition::e_TIMED_OUT ==
                d_condition.timedWait(&d_mutex, absoluteTime)) {
                return false;
            }
        }
        return true;
    }

    typename Future<T>::Maker generateMaker()
    {
        return bdlf::BindUtil::bind(&Future<T>::Impl::made,
                                    Future<T>::Impl::weak_from_this(),
                                    bdlf::PlaceHolders::_1);
    }

    const Result<T>& result(bslmt::LockGuard<bslmt::Mutex>* haveGuard = 0)
    {
        if (!haveGuard) {
            bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
            return resultNoLock();
        }
        BSLS_ASSERT(haveGuard->ptr() == &d_mutex);
        return resultNoLock();
    }

    template <typename newT>
    Future<newT>
    addChain(const bsl::function<Result<newT>(const Result<T>&)>& newTConverter)
    {
        typename rmqt::Future<newT>::Pair futurePair(
            rmqt::Future<newT>::make(getLifetimeExtension()));

        bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
        if (d_done) {
            converter(futurePair.first, newTConverter, result(&guard));
        }
        else {
            d_chain.push_back(
                bdlf::BindUtil::bind(&Future<T>::Impl::converter<newT>,
                                     futurePair.first,
                                     newTConverter,
                                     bdlf::PlaceHolders::_1));
        }

        return futurePair.second;
    }

    template <typename newT>
    Future<newT>
    addChain(const bsl::function<Future<newT>(const Result<T>&)>& futureMaker)
    {
        typename rmqt::Future<newT>::Pair futurePair(
            rmqt::Future<newT>::make(getLifetimeExtension()));

        bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
        if (d_done) {
            futureConverter<newT>(
                futureMaker,
                futurePair.first,
                bsl::weak_ptr<typename rmqt::Future<newT>::Impl>(
                    futurePair.second.d_impl),
                result(&guard));
        }
        else {
            d_chain.push_back(bdlf::BindUtil::bind(
                &Future<T>::Impl::futureConverter<newT>,
                futureMaker,
                futurePair.first,
                bsl::weak_ptr<typename rmqt::Future<newT>::Impl>(
                    futurePair.second.d_impl),
                bdlf::PlaceHolders::_1));
        }
        return futurePair.second;
    }

    bsl::function<void()> getLifetimeExtension()
    {
        bsl::function<void(const bsl::shared_ptr<typename Future<T>::Impl>&)>
            func;
        func = &Future<T>::Impl::extendChainParentLifetime;
        return bdlf::BindUtil::bind(func, Future<T>::Impl::shared_from_this());
    }

    void updateCancel(const bsl::function<void()>& newCanc)
    {
        bslmt::LockGuard<bslmt::Mutex> guard(&d_mutex);
        d_cancelFunc = newCanc;
    }

  private:
    const Result<T>& resultNoLock()
    {
        BSLS_ASSERT(d_done);
        return d_result;
    }

    void notifyChain(bslmt::LockGuard<bslmt::Mutex>* haveGuard)
    {
        BSLS_ASSERT(haveGuard);
        typename bsl::list<bsl::function<void(const Result<T>&)> >::iterator it;
        for (it = d_chain.begin(); it != d_chain.end(); ++it) {
            (*it)(result(haveGuard));
        }
        d_chain.clear();
    }

  private:
    bslmt::Mutex d_mutex;
    bslmt::Condition d_condition;
    Result<T> d_result;
    bsl::list<typename rmqt::Future<T>::Maker> d_chain;
    bool d_done;

    // Called if the future is not completed when Future<T>::Impl destructs
    // This function is used to extend the lifetime of chained futures.
    // See extendChainParentLifetime
    bsl::function<void()> d_cancelFunc;
};

} // namespace rmqt
} // namespace BloombergLP

#endif
