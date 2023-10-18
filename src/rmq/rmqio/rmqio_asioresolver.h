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

// rmqio_asioresolver.h
#ifndef INCLUDED_RMQIO_ASIORESOLVER_H
#define INCLUDED_RMQIO_ASIORESOLVER_H

//@PURPOSE: Resolver Asio impl
//
//@CLASSES:
// Resolver
//
//@AUTHOR: whoy1
//

// Includes
#include <rmqio_resolver.h>

#include <rmqio_asioeventloop.h>
#include <rmqio_asiosocketwrapper.h>

#include <rmqt_result.h>
#include <rmqt_securityparameters.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <bdlb_random.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {
class SecurityParameters;
}

namespace rmqio {

template <typename SocketType>
class AsioConnection;

class CustomRandomGenerator {
  public:
    CustomRandomGenerator(int seed)
    : d_seed(seed)
    {
    }

    bsl::ptrdiff_t operator()(bsl::ptrdiff_t max)
    {
        int rand = bdlb::Random::generate15(&d_seed) % static_cast<int>(max);
        return static_cast<bsl::ptrdiff_t>(rand);
    }

  private:
    int d_seed;
};

//=======
// Class AsioResolver
//=======

class AsioResolver : public Resolver,
                     public bsl::enable_shared_from_this<AsioResolver> {
  public:
    static bsl::shared_ptr<AsioResolver>
    create(AsioEventLoop& eventloop, bool shuffleConnectionEndpoints);

    virtual bsl::shared_ptr<Connection>
    asyncConnect(const bsl::string& host,
                 bsl::uint16_t port,
                 bsl::size_t maxFrameSize,
                 const Connection::Callbacks& connCallbacks,
                 const NewConnectionCallback& onSuccess,
                 const ErrorCallback& onFail);

    virtual bsl::shared_ptr<Connection> asyncSecureConnect(
        const bsl::string& host,
        bsl::uint16_t port,
        bsl::size_t maxFrameSize,
        const bsl::shared_ptr<rmqt::SecurityParameters>& securityParameters,
        const Connection::Callbacks& connCallbacks,
        const NewConnectionCallback& onSuccess,
        const ErrorCallback& onFail);

    typedef boost::asio::ip::tcp::resolver::results_type results_type;

    static void shuffleResolverResults(results_type& resolverResults,
                                       bool shuffleConnectionEndpoints,
                                       CustomRandomGenerator& g,
                                       const bsl::string& host,
                                       const bsl::string& port);

  private:
    explicit AsioResolver(AsioEventLoop& eventloop,
                          bool shuffleConnectionEndpoints);
    template <typename SocketType>
    void startConnect(
        const bsl::string& host,
        AsioResolver::results_type resolverResults,
        const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
        const bsl::weak_ptr<SocketType>& weakSocket,
        const NewConnectionCallback& onSuccess,
        const ErrorCallback& onFail);

    template <typename SocketType>
    void handleConnect(
        const bsl::string& host,
        boost::system::error_code error,
        AsioResolver::results_type::iterator endpoint,
        const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
        const NewConnectionCallback& onSuccess,
        const ErrorCallback& onFail);

    template <typename SocketType>
    static void handleConnectCb(
        bsl::weak_ptr<AsioResolver> weakSelf,
        const bsl::string& host,
        boost::system::error_code error,
        AsioResolver::results_type::iterator endpoint,
        const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
        const bsl::shared_ptr<SocketType>& socketLifetime,
        const NewConnectionCallback& onSuccess,
        const ErrorCallback& onFail);

    template <typename SocketType>
    static void handleHandshakeCb(
        bsl::weak_ptr<AsioResolver> weakSelf,
        boost::system::error_code error,
        const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
        const bsl::shared_ptr<SocketType>& socketLifetime,
        const NewConnectionCallback& onSuccess,
        const ErrorCallback& onFail);

    template <typename SocketType>
    void handleResolve(
        const bsl::string& host,
        const bsl::string& port,
        boost::system::error_code error,
        AsioResolver::results_type resolverResults,
        const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
        const bsl::weak_ptr<SocketType>& weakSocket,
        const NewConnectionCallback& onSuccess,
        const ErrorCallback& onFail);

    template <typename SocketType>
    static void handleResolveCb(
        bsl::weak_ptr<AsioResolver> weakSelf,
        const bsl::string& host,
        const bsl::string& port,
        boost::system::error_code error,
        AsioResolver::results_type resolver_results,
        const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
        const bsl::weak_ptr<SocketType>& weakSocket,
        const NewConnectionCallback& onSuccess,
        const ErrorCallback& onFail);

    bool shuffleConnectionEndpoints() const
    {
        return d_shuffleConnectionEndpoints;
    }

  private:
    boost::asio::ip::tcp::resolver d_resolver;
    bool d_shuffleConnectionEndpoints;
};

} // namespace rmqio
} // namespace BloombergLP
#endif
