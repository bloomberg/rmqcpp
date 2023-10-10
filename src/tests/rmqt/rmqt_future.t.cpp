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

#include <rmqt_future.h>

#include <rmqt_result.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bslmt_threadutil.h>

#include <bsl_cstddef.h>
#include <bsl_optional.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_vector.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace BloombergLP;
using namespace ::testing;

namespace {

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQT.FUTURE.TESTS")

// For operations which we expect to timeout
const double WAIT_TIMEOUT = 0.075;

// For operations we don't expect to hit the timeout for
const double LONG_WAIT_TIMEOUT = 10.0;

template <typename T>
void makeMyTLocally(const typename rmqt::Future<T>::Maker& maker,
                    const rmqt::Result<T>& t,
                    int waitMicroSeconds = 0)
{
    bslmt::ThreadUtil::microSleep(waitMicroSeconds);
    maker(t);
}
template <typename T>
void waitLocally(typename rmqt::Future<T> future, T& theT)
{
    BALL_LOG_DEBUG << "blockResult";
    rmqt::Result<T> result = future.blockResult();
    if (result) {
        theT = *result.value();
        BALL_LOG_DEBUG << "gotResult";
    }
    else {
        BALL_LOG_DEBUG << "badResult";
    }
}

template <typename T>
class FutureMockUtil {
  public:
    MOCK_METHOD0_T(futureWork, rmqt::Future<T>());
    MOCK_METHOD1_T(futureWorkWithItem,
                   rmqt::Future<T>(const bsl::shared_ptr<T>&));

    MOCK_METHOD0(cancel, void());
    MOCK_METHOD1_T(called, void(const rmqt::Result<T>&));

    template <typename newT>
    rmqt::Result<newT> processResult(const rmqt::Result<T>& t)
    {
        called(t);
        return rmqt::Result<newT>("mock");
    }
};

class FutureTesting : public ::testing::Test {
  public:
    template <typename T>
    void makeMyT(const typename rmqt::Future<T>::Maker& maker,
                 const rmqt::Result<T>& t,
                 int waitMicroSeconds)
    {
        bslmt::ThreadUtil::create(
            &d_makerThread,
            bdlf::BindUtil::bind(
                &makeMyTLocally<T>, maker, t, waitMicroSeconds));
    }

    template <size_t threads, typename T>
    void createWaitersFor(const rmqt::Future<T>& future)
    {
        d_results.resize(threads);
        d_threads.resize(threads);
        for (size_t i = 0; i < threads; ++i) {
            bslmt::ThreadUtil::create(
                &d_threads[i],
                bdlf::BindUtil::bind(
                    &waitLocally<T>, future, bsl::ref(d_results[i])));
        }
    }

    void joinThreads()
    {
        for (size_t i = 0; i < d_threads.size(); ++i) {
            bslmt::ThreadUtil::join(d_threads[i]);
        }
        bsl::vector<bslmt::ThreadUtil::Handle>().swap(d_threads);
    }

    template <typename T>
    rmqt::Future<T> futureIn(
        int microseconds,
        const rmqt::Result<T>& result = rmqt::Result<T>(bsl::make_shared<T>()),
        bool expectCancel             = false)
    {
        typename rmqt::Future<T>::Pair futurePair =
            expectCancel ? rmqt::Future<T>::make(bdlf::BindUtil::bind(
                               &FutureMockUtil<int>::cancel, &d_cancel))
                         : rmqt::Future<T>::make();
        if (expectCancel) {
            EXPECT_CALL(d_cancel, cancel()).Times(1);
        }
        makeMyT(futurePair.first, result, microseconds);

        return futurePair.second;
    }

    ~FutureTesting()
    {
        try {
            joinThreads();
        }
        catch (...) {
        }
    }
    bslmt::ThreadUtil::Handle d_makerThread;
    bsl::vector<bslmt::ThreadUtil::Handle> d_threads;
    bsl::vector<int> d_results;
    FutureMockUtil<int> d_cancel;
};

} // namespace

TEST(Future, Breathing)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();
}

TEST(Future, BreathingWithCancel)
{
    FutureMockUtil<void> futureMock;
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make(
        bdlf::BindUtil::bind(&FutureMockUtil<void>::cancel, &futureMock));

    EXPECT_CALL(futureMock, cancel());
}

TEST(Future, tryResultFail)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();

    rmqt::Result<int> result = foo.second.tryResult();

    bool fail = !result;
    EXPECT_THAT(fail, Eq(true));
    bool pointerIsNull = !result.value();

    EXPECT_THAT(pointerIsNull, Eq(true));
}

TEST(Future, tryResultSuccess)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();

    // run inline
    makeMyTLocally(
        foo.first, rmqt::Result<int>(bsl::make_shared<int>(1337)), 0);
    rmqt::Result<int> result = foo.second.tryResult();

    bool fail = !result;
    EXPECT_THAT(fail, Eq(false));
    EXPECT_THAT(*result.value(), Eq(1337));
}

TEST(Future, tryResultErrorSuccess)
{
    rmqt::Future<>::Pair foo = rmqt::Future<>::make();

    // run inline
    makeMyTLocally(foo.first, rmqt::Result<>("ERROR"));
    rmqt::Result<> result = foo.second.tryResult();
    bool fail             = !result;
    EXPECT_THAT(fail, Eq(true));
    EXPECT_THAT(result.error(), Eq("ERROR"));
}

TEST_F(FutureTesting, tryResultSuccess)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();

    makeMyT(foo.first, rmqt::Result<int>(bsl::make_shared<int>(30)), 10);
    rmqt::Result<int> result =
        foo.second.waitResult(bsls::TimeInterval(LONG_WAIT_TIMEOUT));

    bool success = result;
    EXPECT_THAT(success, Eq(true));
    EXPECT_THAT(*result.value(), Eq(30));
}

TEST_F(FutureTesting, tryResultTimeoutFailVoid)
{
    rmqt::Future<>::Pair foo = rmqt::Future<>::make();

    rmqt::Result<> result =
        foo.second.waitResult(bsls::TimeInterval(WAIT_TIMEOUT));

    bool success = result;
    EXPECT_THAT(success, Eq(false));
}

TEST_F(FutureTesting, tryResultTimeoutFail)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();

    rmqt::Result<int> result =
        foo.second.waitResult(bsls::TimeInterval(WAIT_TIMEOUT));

    bool success = result;
    EXPECT_THAT(success, Eq(false));
    EXPECT_THAT(result.returnCode(), Eq(static_cast<int>(rmqt::TIMEOUT)));
    EXPECT_THAT(result.error(), Eq("TIMED OUT"));
    EXPECT_THAT(!result.value(), Eq(true));
}

TEST_F(FutureTesting, tryResultError)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();
    makeMyT(foo.first, rmqt::Result<int>("Error"), 0);
    rmqt::Result<int> result =
        foo.second.waitResult(bsls::TimeInterval(LONG_WAIT_TIMEOUT));

    bool success = result;
    EXPECT_THAT(success, Eq(false));
    EXPECT_THAT(!result.value(), Eq(true));
    EXPECT_THAT(result.error(), Eq("Error"));
}

TEST_F(FutureTesting, blockResult)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();
    makeMyT(
        foo.first, rmqt::Result<int>(bsl::make_shared<int>(0xdeadbeef)), 500);
    rmqt::Result<int> result = foo.second.blockResult();

    bool success = result;
    EXPECT_THAT(success, Eq(true));
    EXPECT_THAT(*result.value(), Eq(0xdeadbeef));
    EXPECT_THAT(result.error(), Eq(""));
}
TEST_F(FutureTesting, blockResultError)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();
    makeMyT(foo.first, rmqt::Result<int>("Error"), 100);
    rmqt::Result<int> result = foo.second.blockResult();

    bool fail = !result;
    EXPECT_THAT(fail, Eq(true));
    bool pointerIsNull = !result.value();

    EXPECT_THAT(pointerIsNull, Eq(true));

    EXPECT_THAT(result.error(), Eq("Error"));
}

TEST_F(FutureTesting, blockResultAfterMade)
{
    rmqt::Future<int>::Pair foo = rmqt::Future<int>::make();

    makeMyTLocally(
        foo.first, rmqt::Result<int>(bsl::make_shared<int>(0xace0cafe)), 100);
    rmqt::Result<int> result = foo.second.blockResult();

    bool fail = !result;
    EXPECT_THAT(fail, Eq(false));
    EXPECT_THAT(*result.value(), Eq(0xace0cafe));
    EXPECT_THAT(result.error(), Eq(""));
}

TEST_F(FutureTesting, copyFutures)
{
    rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();
    rmqt::Future<int> future            = resultMaker.second;

    createWaitersFor<5>(future);

    rmqt::Future<int> newFuture(future);
    makeMyTLocally(resultMaker.first,
                   rmqt::Result<int>(bsl::make_shared<int>(1024)));

    joinThreads();

    EXPECT_THAT(*future.blockResult().value(), Eq(1024));
    EXPECT_THAT(*future.blockResult().value(), Eq(1024));

    size_t count = 0;
    for (size_t i = 0; i < d_results.size(); ++i) {
        count += d_results[i];
    }
    EXPECT_THAT(count, Eq(5 * 1024));
}
namespace {
rmqt::Result<double> convert(const rmqt::Result<int>& a)
{
    return rmqt::Result<double>(bsl::make_shared<double>(*a.value() + 1));
}

rmqt::Future<int> asyncIncrement(const rmqt::Result<int>& result)
{
    rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();

    resultMaker.first(
        rmqt::Result<int>(bsl::make_shared<int>(*result.value() + 1)));
    return resultMaker.second;
}

rmqt::Future<bsl::string>
asyncIncrementToString(const rmqt::Result<int>& result)
{
    bsl::ostringstream oss;
    oss << (*result.value()) + 1;
    return rmqt::Future<bsl::string>(
        rmqt::Result<bsl::string>(bsl::make_shared<bsl::string>(oss.str())));
}

} // namespace

TEST_F(FutureTesting, then)
{
    rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();
    rmqt::Future<int> future            = resultMaker.second;

    rmqt::Future<double> newFuture = future.then<double>(&convert);

    makeMyTLocally(resultMaker.first,
                   rmqt::Result<int>(bsl::make_shared<int>(1024)));

    EXPECT_THAT(*newFuture.blockResult().value(), Eq(1025.0));
    EXPECT_THAT(*future.blockResult().value(), Eq(1024));
}

TEST_F(FutureTesting, thenFuture)
{
    rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();
    rmqt::Future<int> firstFuture       = resultMaker.second;
    rmqt::Future<int> secondFuture =
        firstFuture.thenFuture<int>(&asyncIncrement);
    rmqt::Future<int> thirdFuture =
        secondFuture.thenFuture<int>(&asyncIncrement);

    makeMyTLocally(resultMaker.first,
                   rmqt::Result<int>(bsl::make_shared<int>(42)));

    EXPECT_THAT(*firstFuture.blockResult().value(), Eq(42));
    EXPECT_THAT(*secondFuture.blockResult().value(), Eq(43));
    EXPECT_THAT(*thirdFuture.blockResult().value(), Eq(44));
}

TEST_F(FutureTesting, thenFutureDiffTypes)
{
    rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();
    rmqt::Future<int> firstFuture       = resultMaker.second;
    rmqt::Future<bsl::string> secondFuture =
        firstFuture.thenFuture<bsl::string>(&asyncIncrementToString);
    makeMyTLocally(resultMaker.first,
                   rmqt::Result<int>(bsl::make_shared<int>(42)));
    EXPECT_THAT(*firstFuture.blockResult().value(), Eq(42));
    EXPECT_THAT(*secondFuture.blockResult().value(), Eq("43"));
}

TEST_F(FutureTesting, thenFutureAfterResolve)
{
    rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();
    rmqt::Future<int> firstFuture       = resultMaker.second;

    makeMyTLocally(resultMaker.first,
                   rmqt::Result<int>(bsl::make_shared<int>(42)));

    rmqt::Future<int> secondFuture =
        firstFuture.thenFuture<int>(&asyncIncrement);
    rmqt::Future<int> thirdFuture =
        secondFuture.thenFuture<int>(&asyncIncrement);

    EXPECT_THAT(*firstFuture.blockResult().value(), Eq(42));
    EXPECT_THAT(*secondFuture.blockResult().value(), Eq(43));
    EXPECT_THAT(*thirdFuture.blockResult().value(), Eq(44));
}

TEST_F(FutureTesting, chainAfterMade)
{
    rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();
    rmqt::Future<int> future            = resultMaker.second;

    makeMyTLocally(resultMaker.first,
                   rmqt::Result<int>(bsl::make_shared<int>(1024)));

    rmqt::Future<double> newFuture(future.then<double>(&convert));

    EXPECT_THAT(*future.blockResult().value(), Eq(1024));

    EXPECT_THAT(*newFuture.blockResult().value(), Eq(1025.0));
}

namespace {
rmqt::Result<int> passthroughChain(const rmqt::Result<int>& x) { return x; }

} // namespace

TEST_F(FutureTesting, chainUseCount)
{
    bsl::shared_ptr<int> useCountCheck = bsl::make_shared<int>(5);
    {
        rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();

        rmqt::Future<int> second =
            resultMaker.second.then<int>(&passthroughChain);

        resultMaker.first(rmqt::Result<int>(useCountCheck));
    }

    EXPECT_THAT(useCountCheck.use_count(), Eq(1));
}

TEST_F(FutureTesting, chainAfterResolveUseCount)
{
    bsl::shared_ptr<int> useCountCheck = bsl::make_shared<int>(5);
    {
        rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();

        resultMaker.first(rmqt::Result<int>(useCountCheck));

        rmqt::Future<int> second =
            resultMaker.second.then<int>(&passthroughChain);
    }

    EXPECT_THAT(useCountCheck.use_count(), Eq(1));
}

TEST_F(FutureTesting, chainResolveAfterDestruction)
{
    bsl::shared_ptr<int> useCountCheck = bsl::make_shared<int>(5);

    {
        rmqt::Future<int>::Maker maker;
        {
            rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();

            maker = resultMaker.first;

            rmqt::Future<int> second =
                resultMaker.second.then<int>(&passthroughChain);
        }

        maker(rmqt::Result<int>(useCountCheck));
    }

    EXPECT_THAT(useCountCheck.use_count(), Eq(1));
}

TEST_F(FutureTesting, destructionGivesUpOwnership)
{
    bsl::shared_ptr<int> useCountCheck = bsl::make_shared<int>(5);

    {
        rmqt::Future<int>::Maker maker;
        {
            rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();

            maker = resultMaker.first;

            // future is cancelled when it goes out of scope here
        }

        // Resolving here should not affect useCount
        maker(rmqt::Result<int>(useCountCheck));

        EXPECT_THAT(useCountCheck.use_count(), Eq(1));
    }

    EXPECT_THAT(useCountCheck.use_count(), Eq(1));
}

TEST_F(FutureTesting, chainKeepsFutureParentAlive)
{
    {
        rmqt::Future<int>::Maker maker;
        bsl::optional<rmqt::Future<int> > chainedFuture;
        {
            rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make();

            maker = resultMaker.first;

            chainedFuture = resultMaker.second.then<int>(&passthroughChain);

            // Original future is cancelled when it goes out of scope here
        }

        // Resolving here should still work
        maker(rmqt::Result<int>(bsl::make_shared<int>(5)));

        EXPECT_TRUE(chainedFuture.value().blockResult());
    }
}
TEST_F(FutureTesting, thenKeepsFutureParentAliveButCallsCancelOnDestruction)
{
    StrictMock<FutureMockUtil<int> > mockUtil;
    EXPECT_CALL(mockUtil, cancel()).Times(1);
    {
        rmqt::Future<int>::Maker maker;
        bsl::optional<rmqt::Future<int> > chainedFuture;
        {
            rmqt::Future<int>::Pair resultMaker = rmqt::Future<int>::make(
                bdlf::BindUtil::bind(&FutureMockUtil<int>::cancel, &mockUtil));

            maker = resultMaker.first;

            chainedFuture = resultMaker.second.then<int>(&passthroughChain);

            // Original future is cancelled when it goes out of scope here
        }

        EXPECT_FALSE(chainedFuture.value().tryResult());
    }
}
TEST_F(FutureTesting,
       thenFutureKeepsFutureParentAliveButCallsCancelOnDestruction)
{
    bsl::optional<rmqt::Future<int> > chainedFuture;
    {
        rmqt::Future<int> foo =
            futureIn<int>(300000, rmqt::Result<int>("Fail"), true);

        chainedFuture = foo.thenFuture<int>(&asyncIncrement);
    }
    EXPECT_FALSE(chainedFuture.value().tryResult());
    EXPECT_THAT(chainedFuture.value().tryResult().error(), Ne("Fail"));
    chainedFuture.reset();
}

TEST_F(FutureTesting, propagateFailureTestNoFail)
{

    FutureMockUtil<int> mockUtil;

    EXPECT_CALL(mockUtil, futureWorkWithItem(_))
        .WillOnce(Return(
            rmqt::Future<int>(rmqt::Result<int>(bsl::make_shared<int>(4)))));

    bsl::function<rmqt::Future<int>(const bsl::shared_ptr<int>&)> fn =
        bdlf::BindUtil::bind(&FutureMockUtil<int>::futureWorkWithItem,
                             &mockUtil,
                             bdlf::PlaceHolders::_1);

    bsl::function<rmqt::Future<int>(const rmqt::Result<int>&)> futurefunc =
        rmqt::FutureUtil::propagateError<int, int>(fn);

    rmqt::Future<int> future =
        futurefunc(rmqt::Result<int>(bsl::make_shared<int>(5)));

    rmqt::Result<int> result = future.blockResult();
    EXPECT_TRUE(result);
    EXPECT_THAT(*result.value(), Eq(4));
}

TEST_F(FutureTesting, propagateFailureTestWithFail)
{

    StrictMock<FutureMockUtil<int> > mockUtil;

    rmqt::Future<int> future = rmqt::FutureUtil::propagateError<int, int>(
        bdlf::BindUtil::bind(&FutureMockUtil<int>::futureWork, &mockUtil))(
        rmqt::Result<int>("what a fail"));

    rmqt::Result<int> result = future.blockResult();
    EXPECT_FALSE(result);
    EXPECT_THAT(result.error(), Eq("what a fail"));
}

TEST_F(FutureTesting, propagateFailureWithItemTest)
{

    FutureMockUtil<int> mockUtil;

    EXPECT_CALL(mockUtil, futureWorkWithItem(_))
        .WillOnce(Return(
            rmqt::Future<int>(rmqt::Result<int>(bsl::make_shared<int>(4)))));
    rmqt::Future<int> future = rmqt::FutureUtil::propagateError<int, int>(
        bdlf::BindUtil::bind(&FutureMockUtil<int>::futureWorkWithItem,
                             &mockUtil,
                             bdlf::PlaceHolders::_1))(
        rmqt::Result<int>(bsl::make_shared<int>(5)));
    rmqt::Result<int> result = future.blockResult();
    EXPECT_TRUE(result);
    EXPECT_THAT(*result.value(), Eq(4));

    StrictMock<FutureMockUtil<int> > strictMockUtil;

    future = rmqt::FutureUtil::propagateError<int, int>(bdlf::BindUtil::bind(
        &FutureMockUtil<int>::futureWorkWithItem,
        &strictMockUtil,
        bdlf::PlaceHolders::_1))(rmqt::Result<int>("what a fail!"));

    result = future.blockResult();
    EXPECT_FALSE(result);
    EXPECT_THAT(result.error(), Eq("what a fail!"));
}

TEST_F(FutureTesting, unravel)
{
    bslma::ManagedPtr<rmqt::Future<> > unravelledFuture;
    {
        unravelledFuture.load(
            new rmqt::Future<>(rmqt::FutureUtil::unravelFuture<void>()(
                rmqt::Result<rmqt::Future<> >(bsl::make_shared<rmqt::Future<> >(
                    futureIn<void>(500, rmqt::Result<>()))))));
    }
    EXPECT_TRUE(unravelledFuture->blockResult());
}

TEST_F(FutureTesting, unravelCancels)
{
    bslma::ManagedPtr<rmqt::Future<> > unravelledFuture;
    {
        unravelledFuture.load(
            new rmqt::Future<>(rmqt::FutureUtil::unravelFuture<void>()(
                rmqt::Result<rmqt::Future<> >(bsl::make_shared<rmqt::Future<> >(
                    futureIn<void>(5000000, rmqt::Result<>(), true))))));
    }
    EXPECT_FALSE(unravelledFuture->tryResult());
}

TEST_F(FutureTesting, flatten)
{
    bslma::ManagedPtr<rmqt::Future<> > flattenedFuture;
    {
        rmqt::Future<rmqt::Future<> > futurefuture = futureIn<rmqt::Future<> >(
            100,
            rmqt::Result<rmqt::Future<> >(bsl::make_shared<rmqt::Future<> >(
                futureIn<void>(200, rmqt::Result<>()))));
        flattenedFuture.load(
            new rmqt::Future<>(rmqt::FutureUtil::flatten<>(futurefuture)));
    }
    EXPECT_TRUE(flattenedFuture->blockResult());
}

TEST_F(FutureTesting, flattenWithFailOnInnerFuture)
{
    bslma::ManagedPtr<rmqt::Future<> > flattenedFuture;
    {
        rmqt::Future<rmqt::Future<> > futurefuture = futureIn<rmqt::Future<> >(
            100,
            rmqt::Result<rmqt::Future<> >(bsl::make_shared<rmqt::Future<> >(
                futureIn<void>(200, rmqt::Result<>("Fail")))));
        flattenedFuture.load(
            new rmqt::Future<>(rmqt::FutureUtil::flatten<>(futurefuture)));
    }
    EXPECT_FALSE(flattenedFuture->blockResult());
    EXPECT_THAT(flattenedFuture->blockResult().error(), Eq("Fail"));
}

TEST_F(FutureTesting, flattenWithFailOnOuterFuture)
{
    bslma::ManagedPtr<rmqt::Future<> > flattenedFuture;
    {
        rmqt::Future<rmqt::Future<> > futurefuture = futureIn<rmqt::Future<> >(
            100, rmqt::Result<rmqt::Future<> >("Fail"));
        flattenedFuture.load(
            new rmqt::Future<>(rmqt::FutureUtil::flatten<>(futurefuture)));
    }
    EXPECT_FALSE(flattenedFuture->blockResult());
    EXPECT_THAT(flattenedFuture->blockResult().error(), Eq("Fail"));
}

TEST_F(FutureTesting, flattenWithReallyLongMakeCleansUp)
{
    int reallyLongTime = 100000000;
    bslma::ManagedPtr<rmqt::Future<> > flattenedFuture;
    {
        rmqt::Future<rmqt::Future<> > futurefuture = futureIn<rmqt::Future<> >(
            reallyLongTime,
            rmqt::Result<rmqt::Future<> >(bsl::make_shared<rmqt::Future<> >(
                futureIn<void>(reallyLongTime, rmqt::Result<>(), false))),
            true);
        flattenedFuture.load(
            new rmqt::Future<>(rmqt::FutureUtil::flatten<>(futurefuture)));
    }
    EXPECT_FALSE(flattenedFuture->tryResult());
    flattenedFuture.reset();
}
