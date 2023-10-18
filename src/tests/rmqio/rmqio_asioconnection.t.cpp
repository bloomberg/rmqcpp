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

#include <rmqio_asioconnection.h>

#include <rmqio_asioeventloop.h>
#include <rmqio_asiosocketwrapper.h>

#include <rmqt_result.h>

#include <bdlf_bind.h>
#include <boost/asio.hpp>
#include <bslma_managedptr.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdio.h>
#include <bsl_functional.h>
#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace rmqio;
using namespace ::testing;

namespace {

class MockDecoder : public Decoder {
  public:
    MockDecoder()
    : Decoder(20480)
    , totalDecoded(0)
    {
    }
    MOCK_METHOD3(appendBytes,
                 Decoder::ReturnCode(bsl::vector<rmqamqpt::Frame>* outFrames,
                                     const void* buffer,
                                     bsl::size_t bufferLength));

    template <bsl::size_t count>
    Decoder::ReturnCode getVector(bsl::vector<rmqamqpt::Frame>* outFrames,
                                  Unused,
                                  bsl::size_t bytes)
    {
        outFrames->resize(count); // Idempotent
        totalDecoded += bytes;
        return Decoder::OK;
    }
    template <bsl::size_t count>
    Decoder::ReturnCode getVectorBadRC(bsl::vector<rmqamqpt::Frame>* outFrames,
                                       Unused,
                                       bsl::size_t bytes)
    {
        outFrames->resize(count); // Idempotent
        totalDecoded += bytes;
        return Decoder::DECODE_ERROR;
    }
    bsl::size_t totalDecoded;
};

class TestableAsioConnection : public AsioConnection<AsioSocket> {
  public:
    TestableAsioConnection(const Connection::Callbacks& cb,
                           bslma::ManagedPtr<rmqio::Decoder> decoder)
    : AsioConnection<AsioSocket>(
          bsl::shared_ptr<boost::asio::ip::tcp::socket>(),
          cb,
          bsl::ref(decoder))
    {
    }

    bool proxyDoRead(size_t bytes_transferred)
    {
        prepareBuffer();
        return doRead(bytes_transferred);
    }
    void proxyDoClose(Connection::ReturnCode rc) { doClose(rc); }

    void proxyHandleReadError(const boost::system::error_code& error)
    {
        handleReadError(error);
    }

    void proxyHandleError(const boost::system::error_code& error)
    {
        handleError(error);
    }
};

class TestConnection : public Test {
  public:
    class Callbacks {
      public:
        Callbacks()
        : consumeCount(0)
        , errorCount(0)
        , doneCount(0)
        , lastErrorCode(rmqio::Connection::SUCCESS)
        {
        }
        void consumeFrame(const rmqamqpt::Frame&) { ++consumeCount; }
        void errorCallback(Connection::ReturnCode rc)
        {
            ++errorCount;
            lastErrorCode = rc;
        }
        void doneCallback(Connection::ReturnCode) { ++doneCount; }
        bsl::size_t consumeCount;
        bsl::size_t errorCount;
        bsl::size_t doneCount;
        rmqio::Connection::ReturnCode lastErrorCode;
    };

  protected:
    void SetUp()
    {
        Connection::Callbacks callbacks;
        callbacks.onRead =
            bdlf::BindUtil::bind(&TestConnection::Callbacks::consumeFrame,
                                 &d_callbacks,
                                 bdlf::PlaceHolders::_1);
        callbacks.onError =
            bdlf::BindUtil::bind(&TestConnection::Callbacks::errorCallback,
                                 &d_callbacks,
                                 bdlf::PlaceHolders::_1);
        successCount = failCount = 0;
        d_decoder                = bsl::make_shared<MockDecoder>();
        bslma::ManagedPtr<rmqio::Decoder> decoder =
            bslma::ManagedPtr<rmqio::Decoder>(
                d_decoder.get(), NULL, bslma::ManagedPtrUtil::noOpDeleter);
        d_connection = bsl::make_shared<TestableAsioConnection>(
            callbacks, bsl::ref(decoder));
    }

    bsl::size_t successCount;
    bsl::size_t failCount;
    bsl::shared_ptr<MockDecoder> d_decoder;
    TestConnection::Callbacks d_callbacks;
    bsl::shared_ptr<TestableAsioConnection> d_connection;
};

} // namespace

TEST_F(TestConnection, Breathing)
{
    // SetUp tests it
    EXPECT_THAT(d_connection.get(),
                Ne(static_cast<TestableAsioConnection*>(NULL)));
}
// test handle error with success throws
// boost::system::errc::make_error_code(boost::system::errc::success),
TEST_F(TestConnection, OneFrameRead)
{
    EXPECT_THAT(d_callbacks.consumeCount, Eq(0));

    const bsl::size_t BYTES_READ  = 10;
    const bsl::size_t FRAME_COUNT = 1;

    EXPECT_CALL(*d_decoder, appendBytes(Pointee(SizeIs(0)), _, _))
        .WillOnce(
            Invoke(d_decoder.ptr(), &MockDecoder::getVector<FRAME_COUNT>));

    bool readMore = d_connection->proxyDoRead(BYTES_READ);

    EXPECT_THAT(readMore, Eq(true));
    EXPECT_THAT(d_decoder->totalDecoded,
                Eq(BYTES_READ)); // we decoded all that we read
    EXPECT_THAT(d_callbacks.consumeCount,
                Eq(FRAME_COUNT)); // we passed all the frames upstrea
}

TEST_F(TestConnection, FiftyFramesRead)
{
    EXPECT_THAT(d_callbacks.consumeCount, Eq(0));

    const bsl::size_t BYTES_READ  = 500;
    const bsl::size_t FRAME_COUNT = 50;

    EXPECT_CALL(*d_decoder, appendBytes(_, _, _))
        .WillRepeatedly(Invoke(
            d_decoder.ptr(),
            &MockDecoder::getVector<FRAME_COUNT>)); // this could be called
                                                    // more than once

    bool readMore = d_connection->proxyDoRead(BYTES_READ);

    EXPECT_THAT(readMore, Eq(true));
    EXPECT_THAT(d_decoder->totalDecoded,
                Eq(BYTES_READ)); // we decoded all that we read
    EXPECT_THAT(d_callbacks.consumeCount,
                Eq(FRAME_COUNT)); // we passed all the frames upstream
}

TEST_F(TestConnection, MultipleReads)
{
    const bsl::size_t NUMBER_OF_READS = 10;
    EXPECT_THAT(d_callbacks.consumeCount, Eq(0));

    const bsl::size_t BYTES_READ  = 2000;
    const bsl::size_t FRAME_COUNT = 5;

    EXPECT_CALL(*d_decoder, appendBytes(_, _, _))
        .WillRepeatedly(Invoke(
            d_decoder.ptr(),
            &MockDecoder::getVector<FRAME_COUNT>)); // this could be called
                                                    // more than once

    for (size_t i = 0; i < NUMBER_OF_READS; ++i) {
        bool readMore = d_connection->proxyDoRead(BYTES_READ);
        if (!readMore) {
            break;
        }
    }

    EXPECT_THAT(
        d_decoder->totalDecoded,
        Eq(BYTES_READ * NUMBER_OF_READS)); // we decoded all that we read
    EXPECT_THAT(
        d_callbacks.consumeCount,
        Eq(FRAME_COUNT * NUMBER_OF_READS)); // we passed all the frames upstream
}

TEST_F(TestConnection, ShutdownOnBadFrame)
{

    EXPECT_CALL(*d_decoder, appendBytes(_, _, _))
        .WillOnce(Return(Decoder::DECODE_ERROR));
    bool readMore = d_connection->proxyDoRead(100);
    EXPECT_THAT(readMore, Eq(false));
}

TEST_F(TestConnection, SomeGoodFramesSomeBad)
{
    EXPECT_CALL(*d_decoder, appendBytes(_, _, _))
        .WillOnce(Invoke(d_decoder.ptr(), &MockDecoder::getVectorBadRC<1>));

    bool result = d_connection->proxyDoRead(100);

    EXPECT_THAT(result, Eq(false));
    EXPECT_THAT(d_callbacks.consumeCount,
                Eq(1)); // we processed one frame before the decode error
}

TEST_F(TestConnection, SocketSnap)
{
    d_connection->proxyHandleReadError(boost::asio::error::make_error_code(
        boost::asio::error::connection_reset));
    EXPECT_THAT(d_callbacks.consumeCount, Eq(0));
    EXPECT_THAT(d_callbacks.errorCount, Eq(1));
}

TEST_F(TestConnection, SocketClosed)
{
    d_connection->proxyHandleReadError(
        boost::asio::error::make_error_code(boost::asio::error::eof));
    EXPECT_THAT(d_callbacks.consumeCount, Eq(0));
    EXPECT_THAT(d_callbacks.errorCount, Eq(1));
}

class AsioConnectionTests : public Test {

  public:
    void SetUp()
    {
        d_callbacks.onRead =
            bdlf::BindUtil::bind(&TestConnection::Callbacks::consumeFrame,
                                 &d_mockCallbacks,
                                 bdlf::PlaceHolders::_1);
        d_callbacks.onError =
            bdlf::BindUtil::bind(&TestConnection::Callbacks::errorCallback,
                                 &d_mockCallbacks,
                                 bdlf::PlaceHolders::_1);
    }

    boost::asio::io_context d_eventLoop;
    TestConnection::Callbacks d_mockCallbacks;
    Connection::Callbacks d_callbacks;
};

TEST_F(AsioConnectionTests, CloseOpeningSocket)
{
    rmqio::AsioConnection<AsioSocket> conn(
        bsl::make_shared<AsioSocket>(d_eventLoop.get_executor()),
        d_callbacks,
        bslma::ManagedPtr<Decoder>(new MockDecoder()));

    conn.close(bdlf::BindUtil::bind(&TestConnection::Callbacks::doneCallback,
                                    &d_mockCallbacks,
                                    bdlf::PlaceHolders::_1));

    EXPECT_THAT(d_mockCallbacks.doneCount, Eq(1));
}

TEST_F(AsioConnectionTests, MarkOpenNotConnectedSocketErrors)
{
    rmqio::AsioConnection<AsioSocket> conn(
        bsl::make_shared<AsioSocket>(d_eventLoop.get_executor()),
        d_callbacks,
        bslma::ManagedPtr<Decoder>(new MockDecoder()));

    EXPECT_FALSE(conn.markConnected());
}

TEST_F(AsioConnectionTests, DestructSocketCountsAsGracefulClose)
{
    // It's important that an error during AsioConnection destruction is treated
    // as a Graceful Disconnect since rmqamqp::Connection depends on this
    // behaviour to avoid reconnecting during shutdown.

    {
        rmqio::AsioConnection<AsioSocket> conn(
            bsl::make_shared<AsioSocket>(d_eventLoop.get_executor()),
            d_callbacks,
            bslma::ManagedPtr<Decoder>(new MockDecoder()));
    }

    EXPECT_THAT(d_mockCallbacks.errorCount, Eq(1));
    EXPECT_THAT(d_mockCallbacks.lastErrorCode,
                Eq(rmqio::Connection::GRACEFUL_DISCONNECT));
}
