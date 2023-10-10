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

#ifndef INCLUDED_RMQIO_ASIOCONNECTION
#define INCLUDED_RMQIO_ASIOCONNECTION

#include <rmqio_connection.h>
#include <rmqio_decoder.h>
#include <rmqio_serializedframe.h>

#include <bsl_cstddef.h>
#include <bsl_deque.h>
#include <bsl_memory.h>
#include <bsl_optional.h>
#include <bsl_utility.h>
#include <bsl_vector.h>
#include <bslma_managedptr.h>
#include <bsls_keyword.h>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

namespace BloombergLP {
namespace rmqio {

template <typename SocketType>
class AsioConnection
: public Connection,
  public bsl::enable_shared_from_this<AsioConnection<SocketType> > {
  public:
    virtual void
    asyncWrite(const bsl::vector<bsl::shared_ptr<SerializedFrame> >& frames,
               const SuccessWriteCallback&) BSLS_KEYWORD_OVERRIDE;

    virtual void close(const DoneCallback& cb) BSLS_KEYWORD_OVERRIDE;

    virtual bool isConnected() const BSLS_KEYWORD_OVERRIDE;

    AsioConnection(bsl::shared_ptr<SocketType> connecting_socket,
                   const Callbacks& callbacks,
                   bslma::ManagedPtr<Decoder> decoder);

    ~AsioConnection() BSLS_KEYWORD_OVERRIDE;

    /// Indicates that d_socket has established a connection to an endpoint
    bool markConnected();

  protected:
    static void
    handleWriteCb(const bsl::weak_ptr<AsioConnection>& weakSelf,
                  boost::system::error_code error,
                  bsl::size_t bytes_transferred,
                  const bsl::shared_ptr<SocketType>& socketLifetime);
    void handleWrite(boost::system::error_code error,
                     bsl::size_t bytes_transferred);

    static void
    handleReadCb(const bsl::weak_ptr<AsioConnection>& weakSelf,
                 boost::system::error_code error, // Result of operation.
                 bsl::size_t bytes_transferred,   // Number of bytes received.
                 const bsl::shared_ptr<SocketType>& socketLifetime,
                 const bsl::shared_ptr<boost::asio::streambuf>& bufferLifetime);

    void handleRead(boost::system::error_code error, // Result of operation.
                    bsl::size_t bytes_transferred // Number of bytes received.
    );

    bool doRead(bsl::size_t bytes_transferred);

    void handleReadError(boost::system::error_code error);

    bool handleSecureError(boost::system::error_code error);

    void handleError(boost::system::error_code error);

    const boost::asio::streambuf::mutable_buffers_type prepareBuffer();

    static void
    handleCloseCb(const bsl::weak_ptr<AsioConnection>& weakSelf,
                  boost::system::error_code error,
                  const bsl::shared_ptr<SocketType>& socketLifetime);

    void doClose(ReturnCode e = Connection::DISCONNECTED_ERROR);

    void startNextWrite();

    bool startRead();

    enum State { CONNECTING, CONNECTED, CLOSING, DISCONNECTED };

  private:
    bsl::shared_ptr<SocketType> d_socket;
    Callbacks d_callbacks;
    bslma::ManagedPtr<Decoder> d_frameDecoder;
    bsl::optional<DoneCallback> d_shutdown;
    State d_state;
    bsl::shared_ptr<boost::asio::streambuf> d_inbound;

    typedef bsl::pair<SuccessWriteCallback,
                      bsl::vector<bsl::shared_ptr<SerializedFrame> > >
        CallbackDataPair;
    bsl::deque<CallbackDataPair> d_writeQueue;
};

} // namespace rmqio
} // namespace BloombergLP
#endif
