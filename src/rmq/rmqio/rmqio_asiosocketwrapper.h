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

#ifndef INCLUDED_RMQIO_ASIOSOCKETWRAPPER
#define INCLUDED_RMQIO_ASIOSOCKETWRAPPER

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <ball_log.h>
#include <bdlb_variant.h>
#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqio {

typedef boost::asio::ip::tcp::socket AsioSocket;
typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> AsioSSLSocket;

class AsioSecureSocketWrapper {
  public:
    AsioSecureSocketWrapper(
        AsioSSLSocket::executor_type executor,
        const bsl::shared_ptr<boost::asio::ssl::context>& context)
    : d_context(context)
    , d_socket(bsl::make_shared<AsioSSLSocket>(executor, bsl::ref(*d_context)))
    {
    }

    void close()
    {
        boost::system::error_code ec;
        d_socket->lowest_layer().close(ec);

        if (ec) {
            BALL_LOG_SET_CATEGORY("RMQIO.ASIOSOCKETWRAPPER");
            BALL_LOG_ERROR << "Error occurred cancelling tls socket "
                              "reads/writes. Continuing shutdown.";
        }
    }

    AsioSSLSocket::lowest_layer_type& lowest_layer()
    {
        return d_socket->lowest_layer();
    }
    void shutdown(int /*not in ssl api*/, boost::system::error_code& ec)
    {
        // This will cause the receive loop to terminate with short read err,
        // this is the TLS equivalent of EOF
        d_socket->lowest_layer().shutdown(
            boost::asio::ip::tcp::socket::shutdown_receive, ec);
        if (ec) {
            BALL_LOG_SET_CATEGORY("RMQIO.ASIOSOCKETWRAPPER");
            BALL_LOG_WARN << "Failed to Shutdown TLS cleanly: " << ec.message();
            close();
            ec.clear();
        }
    }

    AsioSSLSocket& socket() { return *d_socket; }

  private:
    bsl::shared_ptr<boost::asio::ssl::context> d_context;
    bsl::shared_ptr<AsioSSLSocket> d_socket;
};

} // namespace rmqio
} // namespace BloombergLP
#endif
