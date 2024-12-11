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

#include <rmqio_asiosocketwrapper.h>
#include <rmqt_securityparameters.h>

#include <boost/asio.hpp>
#include <fcntl.h>

#include <ball_log.h>
#include <bdlf_bind.h>
#include <bslma_managedptr.h>
#include <bsls_assert.h>

#include <bsl_algorithm.h>
#include <bsl_cstddef.h>
#include <bsl_memory.h>
#include <bsl_numeric.h>
#include <bsl_sstream.h>
#include <bsl_utility.h>

namespace BloombergLP {
namespace rmqio {

namespace {

BALL_LOG_SET_NAMESPACE_CATEGORY("RMQIO.ASIOCONNECTION")

boost::asio::const_buffer
frame2buffer(const bsl::shared_ptr<SerializedFrame>& frame)
{
    return boost::asio::const_buffer(frame->serialized(), frame->frameLength());
}

bsl::size_t accumulateFun(bsl::size_t totalLen,
                          const bsl::shared_ptr<SerializedFrame>& frame)
{
    return totalLen + frame->frameLength();
}

AsioSocket& use_socket(AsioSocket& socket) { return socket; }

AsioSSLSocket& use_socket(AsioSecureSocketWrapper& wrapper)
{
    return wrapper.socket();
}

template <typename SocketType>
bool prepareSocket(bsl::shared_ptr<SocketType>& socket)
{
#if defined(__sun)
    /* In 32-bit processes, SunOS limits the file descriptors that may be
     * used by the C stdio functions to those under 256. To reduce pressure
     * on these low descriptors, try to use file descriptors >= 256. */
    const int MIN_ACCEPTABLE_SUN_FD = 256;
    int s                           = socket->lowest_layer().native_handle();
    BALL_LOG_DEBUG << "File descriptor initial value is " << s;
    if (s < MIN_ACCEPTABLE_SUN_FD) {
        int newSocket;
        if ((newSocket = fcntl(s, F_DUPFD, 256)) != -1) {
            socket->lowest_layer().close();
            socket->lowest_layer().assign(boost::asio::ip::tcp::v4(),
                                          newSocket);
        }
    }
    BALL_LOG_DEBUG << "File descriptor is "
                   << socket->lowest_layer().native_handle();
#endif

    BALL_LOG_DEBUG << "Turning on TCP_NODELAY & KEEP_ALIVE";
    boost::system::error_code ec;

    // Nagle must be disabled before the first send
    socket->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true), ec);
    if (ec) {
        BALL_LOG_ERROR << "Failed to set socket no_delay";
        return false;
    }

    ec = boost::system::error_code();
    socket->lowest_layer().set_option(
        boost::asio::socket_base::keep_alive(true), ec);
    if (ec) {
        BALL_LOG_ERROR << "Failed to set socket keep_alive";
        return false;
    }

    ec = boost::system::error_code();
    socket->lowest_layer().non_blocking(true, ec);
    if (ec) {
        BALL_LOG_ERROR << "Failed to set socket non-blocking";
        return false;
    }

    return true;
}

} // namespace

template <typename SocketType>
void AsioConnection<SocketType>::asyncWrite(
    const bsl::vector<bsl::shared_ptr<SerializedFrame> >& framePtrs,
    const SuccessWriteCallback& cb)
{
    d_writeQueue.push_back(bsl::make_pair(cb, framePtrs));

    if (d_writeQueue.size() == 1) {
        startNextWrite();
    }
}

template <typename SocketType>
void AsioConnection<SocketType>::startNextWrite()
{
    if (!d_socket) {
        BALL_LOG_WARN << "Attempted to write to closed socket";
        return;
    }

    BSLS_ASSERT(!d_writeQueue.empty());
    const bsl::vector<bsl::shared_ptr<SerializedFrame> >& framePtrs =
        d_writeQueue[0].second;

    bsl::vector<boost::asio::const_buffer> buffers;
    buffers.reserve(framePtrs.size());
    bsl::transform(framePtrs.cbegin(),
                   framePtrs.cend(),
                   bsl::back_inserter(buffers),
                   &frame2buffer);

    boost::asio::async_write(
        use_socket(*d_socket),
        buffers,
        bdlf::BindUtil::bind(&AsioConnection<SocketType>::handleWriteCb,
                             AsioConnection<SocketType>::weak_from_this(),
                             bdlf::PlaceHolders::_1,
                             bdlf::PlaceHolders::_2,
                             d_socket));
}

template <typename SocketType>
bool AsioConnection<SocketType>::markConnected()
{
    if (d_state != CONNECTING) {
        // We probably closed before connecting
        return false;
    }

    d_state = CONNECTED;

    return prepareSocket(d_socket) && startRead();
}

template <typename SocketType>
bool AsioConnection<SocketType>::startRead()
{
    if (!d_socket) {
        BALL_LOG_DEBUG << "Not reading due to socket close";
        return false;
    }

    boost::asio::async_read(
        use_socket(*d_socket),
        prepareBuffer(),
        boost::asio::transfer_at_least(1), // read at least 1byte
        bdlf::BindUtil::bind(&AsioConnection<SocketType>::handleReadCb,
                             AsioConnection<SocketType>::weak_from_this(),
                             bdlf::PlaceHolders::_1,
                             bdlf::PlaceHolders::_2,
                             d_socket,
                             d_inbound));

    return true;
}

template <typename SocketType>
void AsioConnection<SocketType>::close(const DoneCallback& cb)
{
    d_shutdown = cb;
    d_state    = CLOSING;
    boost::system::error_code ec;
    if (d_socket) {
        BALL_LOG_TRACE << "Shutting down socket";
        d_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    }
    if (ec) {
        BALL_LOG_WARN << "Failed to shutdown socket: " << ec;

        d_shutdown.reset();

        cb(DISCONNECTED_ERROR);
    }
}

template <typename SocketType>
AsioConnection<SocketType>::AsioConnection(
    bsl::shared_ptr<SocketType> connecting_socket,
    const Connection::Callbacks& callbacks,
    bslma::ManagedPtr<Decoder> decoder)
: d_socket(connecting_socket)
, d_callbacks(callbacks)
, d_frameDecoder(decoder)
, d_shutdown()
, d_state(CONNECTING)
, d_inbound(bsl::make_shared<boost::asio::streambuf>())
, d_writeQueue()
{
}

template <typename SocketType>
AsioConnection<SocketType>::~AsioConnection()
{
    BALL_LOG_TRACE << "~AsioConnection()";
    if (d_socket) {
        // AsioConnection destructor is an intentional disconnect
        // This error code is important for what we pass back up via
        // `d_callbacks`.
        doClose(GRACEFUL_DISCONNECT);
    }
}

template <typename SocketType>
void AsioConnection<SocketType>::handleWrite(boost::system::error_code error,
                                             bsl::size_t bytes_transferred)

{
    const CallbackDataPair& front = d_writeQueue[0];

    if (!error) {
        BSLS_ASSERT_OPT(bytes_transferred ==
                        bsl::accumulate(front.second.cbegin(),
                                        front.second.cend(),
                                        0u,
                                        &accumulateFun));

        front.first();
    }
    else {
        handleError(error);
    }

    d_writeQueue.pop_front();
    if (!d_writeQueue.empty()) {
        startNextWrite();
    }
}

template <typename SocketType>
void AsioConnection<SocketType>::handleWriteCb(
    const bsl::weak_ptr<AsioConnection>& weakSelf,
    boost::system::error_code error,
    bsl::size_t bytes_transferred,
    const bsl::shared_ptr<SocketType>&)
{
    // Extend socket lifetime to the end of this completion handler for asio

    bsl::shared_ptr<AsioConnection> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_DEBUG
            << "Write callback completed after socket object destruction";
        return;
    }

    self->handleWrite(error, bytes_transferred);
}

template <typename SocketType>
void AsioConnection<SocketType>::handleReadCb(
    const bsl::weak_ptr<AsioConnection>& weakSelf,
    boost::system::error_code error, // Result of operation.
    bsl::size_t bytes_transferred,   // Number of bytes received.
    const bsl::shared_ptr<SocketType>&,
    const bsl::shared_ptr<boost::asio::streambuf>&)
{
    // Extend socket/buffer lifetime to the end of this completion handler

    bsl::shared_ptr<AsioConnection> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_DEBUG
            << "Read callback completed after socket object destruction";
        return;
    }

    self->handleRead(error, bytes_transferred);
}

template <typename SocketType>
void AsioConnection<SocketType>::handleRead(boost::system::error_code error,
                                            bsl::size_t bytes_transferred)

{
    if (!error) {
        if (doRead(bytes_transferred)) {
            startRead(); // read more
        }
        else {
            doClose(FRAME_ERROR);
        }
    }
    else {
        handleReadError(error);
    }
}

template <>
bool AsioConnection<AsioSecureSocketWrapper>::handleSecureError(
    boost::system::error_code)
{
    d_socket->socket().async_shutdown(bdlf::BindUtil::bind(
        &AsioConnection<AsioSecureSocketWrapper>::handleCloseCb,
        AsioConnection<AsioSecureSocketWrapper>::weak_from_this(),
        bdlf::PlaceHolders::_1,
        d_socket));
    return true;
}

template <typename SocketType>
bool AsioConnection<SocketType>::handleSecureError(boost::system::error_code)
{
    return false;
}

template <typename SocketType>
void AsioConnection<SocketType>::handleReadError(
    boost::system::error_code error)
{
    switch (error.value()) {
        case boost::asio::error::eof:
            if (d_state == CLOSING) {
                BALL_LOG_DEBUG << "Received EOF from broker (Already Closing)";
            }
            else {
                BALL_LOG_DEBUG << "Received unexpected EOF from remote peer";
            }
            break;
        case boost::asio::error::operation_aborted:
            if (d_state == CLOSING) {
                BALL_LOG_DEBUG << "Callback Cancelled (operation aborted): "
                                  "graceful shutdown";
            }
            else {
                BALL_LOG_INFO << "Callback Cancelled (operation aborted): "
                                 "unexpected. State: "
                              << d_state;
            }
            break;
        case boost::asio::ssl::error::stream_truncated: // ungraceful TLS stream
                                                        // termination
            if (error.category() == boost::asio::error::get_ssl_category()) {
                bsl::ostringstream tlsError;
                char buf[128] = {};
                ERR_error_string_n(error.value(), buf, sizeof(buf));
                tlsError << buf;

                BALL_LOG_DEBUG << "TLS Short Read: " << tlsError.str();
            }
            break;
        default:
            if (error.category() == boost::asio::error::get_ssl_category()) {
                bsl::ostringstream tlsError;
                tlsError << error.message() << ": ";

                char buf[128] = {};
                ERR_error_string_n(error.value(), buf, sizeof(buf));
                tlsError << buf;

                BALL_LOG_WARN << "TLS Error: " << tlsError.str();
            }
            else {
                BALL_LOG_WARN << "ReadError: " << error;
            }
            break;
    }
    if (d_state == CLOSING) {
        doClose(GRACEFUL_DISCONNECT);
    }
    else {
        handleError(error);
    }
}

template <typename SocketType>
void AsioConnection<SocketType>::handleError(boost::system::error_code error)
{
    switch (error.value()) {
        case boost::asio::error::eof:
            BALL_LOG_DEBUG << "Socket closed: " << error.message()
                           << ". Current state: " << d_state;
            break;
        case boost::asio::error::operation_aborted:
            BALL_LOG_DEBUG << "Socket closed: " << error.message()
                           << ". Current state: " << d_state;
            break;
        default:
            BALL_LOG_WARN << "Socket closed: " << error.message()
                          << ". Current state: " << d_state;
            break;
    }
    if (!handleSecureError(error)) { // returns false if doClose isn't called
                                     // asynchronously already
        doClose();
    }
}

template <typename SocketType>
bool AsioConnection<SocketType>::isConnected() const
{
    return d_state == CONNECTED;
}

template <typename SocketType>
const boost::asio::streambuf::mutable_buffers_type
AsioConnection<SocketType>::prepareBuffer()
{
    return d_inbound->prepare(d_frameDecoder->maxFrameSize());
}

template <typename SocketType>
bool AsioConnection<SocketType>::doRead(bsl::size_t bytes_transferred)
{

    bool success = true;

    // d_inbound is setup with a buffer size of maxFrameSize in ::prepareBuffer
    BSLS_ASSERT(bytes_transferred <= d_frameDecoder->maxFrameSize());

    d_inbound->commit(bytes_transferred);

    bsl::size_t bytes_decoded                       = 0;
    boost::asio::streambuf::const_buffers_type bufs = d_inbound->data();
    bsl::vector<rmqamqpt::Frame> readFrames;
    for (boost::asio::streambuf::const_buffers_type::const_iterator i =
             bufs.begin();
         i != bufs.end();
         ++i) {
        boost::asio::const_buffer buf(*i);
        Decoder::ReturnCode rcode =
            d_frameDecoder->appendBytes(&readFrames, buf.data(), buf.size());
        if (rcode != Decoder::OK) {
            BALL_LOG_WARN << "Bad rcode from decoder: " << rcode;
            // Fail but we still want to process frames we were able to decode
            success = false;
            break;
        };
        bytes_decoded += buf.size();
    }

    if (bytes_decoded != bytes_transferred) {
        BALL_LOG_WARN << "bytes_decoded (" << bytes_decoded
                      << ") != bytes_transferred (" << bytes_transferred << ")";
    }
    bsl::for_each(readFrames.begin(), readFrames.end(), d_callbacks.onRead);
    d_inbound->consume(bytes_decoded);
    return success;
}

template <typename SocketType>
void AsioConnection<SocketType>::handleCloseCb(
    const bsl::weak_ptr<AsioConnection>& weakSelf,
    boost::system::error_code error, // Result of operation.
    const bsl::shared_ptr<SocketType>&)
{
    // Extend socket lifetime to the end of this completion handler
    BALL_LOG_TRACE << "async_shutdown: " << error.message();

    if (error && error.value() != boost::asio::ssl::error::stream_truncated) {
        BALL_LOG_INFO << "rcode with async_shutdown: " << error.message();
    }

    bsl::shared_ptr<AsioConnection> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_DEBUG
            << "Close callback completed after socket object destruction";
        return;
    }

    self->doClose(self->d_state == CLOSING ? GRACEFUL_DISCONNECT
                                           : DISCONNECTED_ERROR);
}

template <typename SocketType>
void AsioConnection<SocketType>::doClose(Connection::ReturnCode rc)
{
    if (d_socket) {
        // This does an ungraceful socket close for both TLS and plain TCP
        // connections
        d_socket->close();
    }
    if (d_state == CLOSING && rc == GRACEFUL_DISCONNECT) {
        if (d_shutdown) {
            d_shutdown.value()(rc);
        }
        else {
            // This happens when the shutdown is triggered by the socket
            // closing
            BALL_LOG_DEBUG << "Graceful disconnect but no shutdown handler";
        }
    }
    d_state = DISCONNECTED;
    d_callbacks.onError(rc);
}

template class AsioConnection<AsioSocket>;
template class AsioConnection<AsioSecureSocketWrapper>;

} // namespace rmqio
} // namespace BloombergLP
