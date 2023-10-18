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

// rmqio_asioresolver.cpp   -*-C++-*-
#include <rmqio_asioresolver.h>

#include <rmqio_asioconnection.h>
#include <rmqt_result.h>
#include <rmqt_securityparameters.h>

#include <ball_log.h>
#include <bdlf_bind.h>

#include <bslma_managedptr.h>
#include <bsls_systemtime.h>
#include <bsls_types.h>

#include <boost/range/algorithm.hpp>
#include <boost/system/error_code.hpp>

#include <bsl_algorithm.h>
#include <bsl_cstdint.h>
#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqio {

//===================
// Class AsioResolver
//===================

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQIO.ASIORESOLVER")

typedef bsl::function<void(const boost::system::error_code& error,
                           AsioResolver::results_type::iterator)>
    ConnectHandler;

void logTlsConnectionAlert(const SSL* s, int where, int ret)
{
    if (where & SSL_CB_ALERT) {
        // Log any TLS alerts sent/received - can be "Unknown CA" etc.
        const char* prefix = (where & SSL_CB_READ) ? "read" : "write";
        switch (ret & 0xff) {
                /*
                 * When we either send or receive a 'Close Notify' it indicates
                 * we/the sender is gracefully closing the TLS connection,
                 * therefore it's normal to hit this codepath and we want to
                 * suppress the trace as a result.
                 */
            case SSL3_AD_CLOSE_NOTIFY:
                BALL_LOG_DEBUG << "SSL alert " << prefix << ":" << ret << ": "
                               << SSL_alert_type_string_long(ret) << ": "
                               << SSL_alert_desc_string_long(ret);
                break;

            default:
                BALL_LOG_WARN << "SSL alert " << prefix << ":" << ret << ": "
                              << SSL_alert_type_string_long(ret) << ": "
                              << SSL_alert_desc_string_long(ret);
        }
    }
    else if (where & SSL_CB_EXIT) {
        // Log error/failure codes of SSL accept/connect calls
        const char* prefix = "undefined";
        const int st_type  = where & ~SSL_ST_MASK;

        if (st_type & SSL_ST_CONNECT) {
            prefix = "SSL_connect";
        }
        else if (st_type & SSL_ST_ACCEPT) {
            prefix = "SSL_accept";
        }

        if (ret == 0) {
            // Errors are reported with ret = 0
            BALL_LOG_ERROR << prefix
                           << " failed in: " << SSL_state_string_long(s);
        }
        else if (ret < 0) {
            // These errors seem to be informational only (i.e. handshake
            // reached a certain step)
            BALL_LOG_DEBUG << prefix
                           << " error in: " << SSL_state_string_long(s);
        }
    }
}

bool logCertVerificationFailure(bool preverified,
                                boost::asio::ssl::verify_context& ctx)
{
    if (!preverified) {
        char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert),
                          subject_name,
                          sizeof(subject_name) - 1);

        BALL_LOG_ERROR << "Certificate verification failed: [" << subject_name
                       << "]: ";
    }

    return preverified;
}

bsl::string augmentTlsError(const boost::system::error_code& ec)
{
    bsl::string err = ec.message();

    if (ec.category() == boost::asio::error::get_ssl_category()) {
        err += " (" + bsl::to_string(ERR_GET_LIB(ec.value())) + "," +
               bsl::to_string(ERR_GET_REASON(ec.value())) + ") ";

        char tempBuf[128];
        ::ERR_error_string_n(ec.value(), tempBuf, sizeof(tempBuf));
        err += tempBuf;
    }

    return err;
}

void handleTLSHandshake(boost::system::error_code error,
                        const bsl::string& host,
                        const Resolver::ErrorCallback& onFail,
                        const ConnectHandler& afterHandshake,
                        AsioResolver::results_type::iterator endpoint)
{
    if (!error) {
        BALL_LOG_DEBUG << " TLS Handshake Complete";
        afterHandshake(error, endpoint); // error code can only be success
    }
    else {
        BALL_LOG_ERROR << "Error Handshaking with [" << host
                       << "]: " << error.value() << " " << error.message();
        onFail(Resolver::ERROR_HANDSHAKE);
    }
}

void startTLSHandshake(
    const bsl::string& host,
    boost::system::error_code error,
    AsioResolver::results_type::iterator endpoint,
    const bsl::shared_ptr<AsioSecureSocketWrapper>& socketWrapper,
    const Resolver::ErrorCallback& onFail,
    const ConnectHandler& connectHandler)
{
    if (!error) {
        BALL_LOG_INFO << "Connected to endpoint: (" << host << ")"
                      << endpoint->endpoint().address() << ":"
                      << endpoint->endpoint().port()
                      << ", starting TLS Handshake";

        socketWrapper->socket().async_handshake(
            boost::asio::ssl::stream_base::client,
            bdlf::BindUtil::bind(&handleTLSHandshake,
                                 bdlf::PlaceHolders::_1,
                                 host,
                                 onFail,
                                 connectHandler,
                                 endpoint));
    }
    else {
        BALL_LOG_ERROR << "Error Connecting [" << host << "]: " << error.value()
                       << " " << error.message();
        onFail(Resolver::ERROR_CONNECT);
    }
}

bsl::shared_ptr<boost::asio::ssl::context>
createSecureContext(const bsl::shared_ptr<rmqt::SecurityParameters>& params)
{
    bsl::shared_ptr<boost::asio::ssl::context> result =
        bsl::make_shared<boost::asio::ssl::context>(
            boost::asio::ssl::context::tls_client);

    bool fail = false;
    boost::system::error_code ec;
    result->set_verify_mode(boost::asio::ssl::verify_peer, ec);
    if (ec) {
        BALL_LOG_ERROR << "Error setting verify mode: " << ec;
        fail = true;
        ec.clear();
    }

    boost::asio::ssl::context::options tls_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::single_dh_use);
    switch (params->method()) {
        case rmqt::SecurityParameters::TLS_1_2_OR_BETTER:
            tls_options |= (boost::asio::ssl::context::no_sslv2 |
                            boost::asio::ssl::context::no_sslv3 |
                            boost::asio::ssl::context::no_tlsv1 |
                            boost::asio::ssl::context::no_tlsv1_1);
            break;
    }

    result->set_options(tls_options, ec);
    if (ec) {
        BALL_LOG_ERROR << "Error setting tls options: " << ec;
        fail = true;
        ec.clear();
    }

    switch (params->verification()) {
        case rmqt::SecurityParameters::MUTUAL:
            result->use_private_key_file(
                params->clientKeyPath(), boost::asio::ssl::context::pem, ec);
            if (ec) {
                BALL_LOG_ERROR << "Error using Client Key File: "
                               << params->clientKeyPath()
                               << ", error: " << augmentTlsError(ec);
                fail = true;
                ec.clear();
            }
            else {
                BALL_LOG_DEBUG << "Using " << params->clientKeyPath()
                               << " for Client Key path";
            }
            result->use_certificate_chain_file(params->clientCertificatePath(),
                                               ec);
            if (ec) {
                BALL_LOG_ERROR << "Error using Client Cert File: "
                               << params->clientCertificatePath()
                               << ", error: " << augmentTlsError(ec);
                fail = true;
                ec.clear();
            }
            else {
                BALL_LOG_DEBUG << "Using " << params->clientCertificatePath()
                               << " for Client Cert path";
            }

        // fall through
        case rmqt::SecurityParameters::VERIFY_SERVER:
            result->load_verify_file(params->certificateAuthorityPath(), ec);
            if (ec) {
                BALL_LOG_ERROR << "Error using CA File: "
                               << params->certificateAuthorityPath()
                               << ", error: " << augmentTlsError(ec);
                fail = true;
                ec.clear();
            }
            else {
                BALL_LOG_DEBUG << "Using " << params->certificateAuthorityPath()
                               << " for verification";
            }
            break;
    }
    SSL_CTX_set_info_callback(result->native_handle(), &logTlsConnectionAlert);

    result->set_verify_callback(&logCertVerificationFailure);

    if (fail) {
        result.reset();
    }
    return result;
}

} // namespace

// CREATORS
bsl::shared_ptr<AsioResolver>
AsioResolver::create(AsioEventLoop& eventloop, bool shuffleConnectionEndpoints)
{
    return bsl::shared_ptr<AsioResolver>(
        new AsioResolver(eventloop, shuffleConnectionEndpoints));
}

AsioResolver::AsioResolver(AsioEventLoop& eventloop,
                           bool shuffleConnectionEndpoints)
: d_resolver(eventloop.context())
, d_shuffleConnectionEndpoints(shuffleConnectionEndpoints)
{
}

bsl::shared_ptr<Connection>
AsioResolver::asyncConnect(const bsl::string& host,
                           bsl::uint16_t port,
                           bsl::size_t maxFrameSize,
                           const Connection::Callbacks& connCallbacks,
                           const NewConnectionCallback& onSuccess,
                           const ErrorCallback& onFail)
{
    bsl::shared_ptr<AsioSocket> socket =
        bsl::make_shared<AsioSocket>(d_resolver.get_executor());

    bslma::ManagedPtr<Decoder> decoder =
        bslma::ManagedPtrUtil::makeManaged<Decoder>(maxFrameSize);

    bsl::shared_ptr<AsioConnection<AsioSocket> > connection =
        bsl::make_shared<AsioConnection<AsioSocket> >(
            socket, connCallbacks, bsl::ref(decoder));

    BALL_LOG_TRACE << "Starting resolution for: " << host << ":" << port;

    d_resolver.async_resolve(
        host.c_str(),
        bsl::to_string(port).c_str(),
        boost::asio::ip::resolver_query_base::numeric_service,
        bdlf::BindUtil::bind(
            &AsioResolver::handleResolveCb<AsioSocket>,
            weak_from_this(),
            host,
            bsl::to_string(port),
            bdlf::PlaceHolders::_1,
            bdlf::PlaceHolders::_2,
            bsl::weak_ptr<AsioConnection<AsioSocket> >(connection),
            bsl::weak_ptr<AsioSocket>(socket),
            onSuccess,
            onFail));

    return connection;
}

bsl::shared_ptr<Connection> AsioResolver::asyncSecureConnect(
    const bsl::string& host,
    bsl::uint16_t port,
    bsl::size_t maxFrameSize,
    const bsl::shared_ptr<rmqt::SecurityParameters>& securityParameters,
    const Connection::Callbacks& connCallbacks,
    const NewConnectionCallback& onSuccess,
    const ErrorCallback& onFail)
{
    bsl::shared_ptr<AsioConnection<AsioSecureSocketWrapper> > connection;
    bsl::shared_ptr<boost::asio::ssl::context> secureContext =
        createSecureContext(securityParameters);
    if (!secureContext) {
        BALL_LOG_ERROR << "Failed to setup TLS client with parameters: ["
                       << *securityParameters << "]";
        onFail(Resolver::ERROR_CONNECT);
        return connection;
    }
    bsl::shared_ptr<AsioSecureSocketWrapper> socket =
        bsl::make_shared<AsioSecureSocketWrapper>(d_resolver.get_executor(),
                                                  secureContext);
    bslma::ManagedPtr<rmqio::Decoder> decoder =
        bslma::ManagedPtrUtil::makeManaged<Decoder>(maxFrameSize);
    connection = bsl::make_shared<AsioConnection<AsioSecureSocketWrapper> >(
        socket, connCallbacks, bsl::ref(decoder));

    BALL_LOG_TRACE << "Starting resolution for: " << host << ":" << port;
    d_resolver.async_resolve(
        host.c_str(),
        bsl::to_string(port).c_str(),
        boost::asio::ip::resolver_query_base::numeric_service,
        bdlf::BindUtil::bind(
            &AsioResolver::handleResolveCb<AsioSecureSocketWrapper>,
            weak_from_this(),
            host,
            bsl::to_string(port),
            bdlf::PlaceHolders::_1,
            bdlf::PlaceHolders::_2,
            bsl::weak_ptr<AsioConnection<AsioSecureSocketWrapper> >(connection),
            bsl::weak_ptr<AsioSecureSocketWrapper>(socket),
            onSuccess,
            onFail));

    return connection;
}

template <>
void AsioResolver::startConnect<AsioSecureSocketWrapper>(
    const bsl::string& host,
    AsioResolver::results_type resolverResults,
    const bsl::weak_ptr<AsioConnection<AsioSecureSocketWrapper> >&
        weakConnection,
    const bsl::weak_ptr<AsioSecureSocketWrapper>& weakSocket,
    const NewConnectionCallback& onSuccess,
    const ErrorCallback& onFail)
{
    bsl::shared_ptr<AsioSecureSocketWrapper> socket = weakSocket.lock();

    if (!socket) {
        BALL_LOG_WARN
            << "DNS resolved after we stopped listening. Is it too slow?";
        return;
    }

    ConnectHandler connectHandler = bdlf::BindUtil::bind(
        &AsioResolver::handleConnectCb<AsioSecureSocketWrapper>,
        weak_from_this(),
        host,
        bdlf::PlaceHolders::_1,
        bdlf::PlaceHolders::_2,
        weakConnection,
        socket, // Holds asio socket alive
        onSuccess,
        onFail);

    boost::asio::async_connect(
        socket->lowest_layer(),
        resolverResults.begin(),
        resolverResults.end(),
        bdlf::BindUtil::bind(&startTLSHandshake,
                             host,
                             bdlf::PlaceHolders::_1,
                             bdlf::PlaceHolders::_2,
                             socket, // Holds asio socket alive
                             onFail,
                             connectHandler));
}

template <typename SocketType>
void AsioResolver::startConnect(
    const bsl::string& host,
    AsioResolver::results_type resolverResults,
    const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
    const bsl::weak_ptr<SocketType>& weakSocket,
    const NewConnectionCallback& onSuccess,
    const ErrorCallback& onFail)
{

    bsl::shared_ptr<SocketType> socket = weakSocket.lock();

    if (!socket) {
        BALL_LOG_WARN << "DNS resolved after we stopped listening. Is "
                         "it too slow?";
        return;
    }

    boost::asio::async_connect(
        *socket,
        resolverResults.begin(),
        resolverResults.end(),
        bdlf::BindUtil::bind(&AsioResolver::handleConnectCb<SocketType>,
                             weak_from_this(),
                             host,
                             bdlf::PlaceHolders::_1,
                             bdlf::PlaceHolders::_2,
                             weakConnection,
                             socket, // Holds asio socket alive
                             onSuccess,
                             onFail));
}

template <typename SocketType>
void AsioResolver::handleConnect(
    const bsl::string& host,
    boost::system::error_code error,
    AsioResolver::results_type::iterator endpoint,
    const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
    const NewConnectionCallback& onSuccess,
    const ErrorCallback& onFail)
{
    if (!error) {
        bsl::shared_ptr<AsioConnection<SocketType> > connection =
            weakConnection.lock();
        if (!connection) {
            BALL_LOG_DEBUG << "Connection established after we stopped "
                              "waiting for it";
            return;
        }

        BALL_LOG_INFO << "Established Connection to " << host << "("
                      << endpoint->endpoint().address()
                      << "):" << endpoint->endpoint().port();

        if (connection->markConnected()) {
            onSuccess();
        }
        else {
            BALL_LOG_ERROR << "Error setting up socket for [" << host << "]: ";
            onFail(Resolver::ERROR_CONNECT);
        }
    }
    else {
        BALL_LOG_ERROR << "Error Connecting [" << host << "]: " << error.value()
                       << " " << error.message();
        onFail(Resolver::ERROR_CONNECT);
    }
}

void AsioResolver::shuffleResolverResults(
    AsioResolver::results_type& resolverResults,
    bool shuffleConnectionEndpoints,
    CustomRandomGenerator& g,
    const bsl::string& host,
    const bsl::string& port)
{
    if (shuffleConnectionEndpoints && resolverResults.size()) {
        typedef boost::asio::ip::basic_resolver_entry<boost::asio::ip::tcp>
            entry_type;
        // 1. copy endpoint entries to a vector to shuffle
        bsl::vector<entry_type> entries(resolverResults.size());
        bsl::copy(
            resolverResults.begin(), resolverResults.end(), entries.begin());

        // 2. shuffle vector (c++ standard dependent)
        boost::range::random_shuffle(entries, g);

        // 3. re-create shuffled results_type
        resolverResults = AsioResolver::results_type::create(
            entries.begin(), entries.end(), host, port);
    }
}

template <typename SocketType>
void AsioResolver::handleResolve(
    const bsl::string& host,
    const bsl::string& port,
    boost::system::error_code error,
    AsioResolver::results_type resolverResults,
    const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
    const bsl::weak_ptr<SocketType>& weakSocket,
    const NewConnectionCallback& onSuccess,
    const ErrorCallback& onFail)
{
    if (!error) {
        BALL_LOG_INFO << "Resolved " << host << ":" << port << " to "
                      << resolverResults.size()
                      << (d_shuffleConnectionEndpoints
                              ? " results, shuffling dns results enabled."
                              : " results.");

        bsls::Types::Int64 totalMicroseconds =
            bsls::SystemTime::nowRealtimeClock().totalMicroseconds();
        int seed = static_cast<int>(totalMicroseconds);
        CustomRandomGenerator g(seed);
        shuffleResolverResults(
            resolverResults, d_shuffleConnectionEndpoints, g, host, port);
        startConnect<SocketType>(host,
                                 resolverResults,
                                 weakConnection,
                                 weakSocket,
                                 onSuccess,
                                 onFail);
    }
    else {
        BALL_LOG_ERROR << "Error Resolving [" << host << "]: " << error.value()
                       << " " << error.message();
        onFail(Resolver::ERROR_RESOLVE);
    }
}

template <typename SocketType>
void AsioResolver::handleConnectCb(
    bsl::weak_ptr<AsioResolver> weakSelf,
    const bsl::string& host,
    boost::system::error_code error,
    AsioResolver::results_type::iterator endpoint,
    const bsl::weak_ptr<AsioConnection<SocketType> >& connection,
    const bsl::shared_ptr<SocketType>& socketLifetime,
    const NewConnectionCallback& onSuccess,
    const ErrorCallback& onFail)
{
    // asio requires socket is kept alive until all completion handlers
    // are called. socketLifetime is only added to the bind function
    // object to achieve this.
    (void)socketLifetime;

    bsl::shared_ptr<AsioResolver> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_DEBUG
            << "Socket connected after resolver destructed, ignoring";
        return;
    }

    self->handleConnect<SocketType>(
        host, error, endpoint, connection, onSuccess, onFail);
}

template <typename SocketType>
void AsioResolver::handleResolveCb(
    bsl::weak_ptr<AsioResolver> weakSelf,
    const bsl::string& host,
    const bsl::string& port,
    boost::system::error_code error,
    AsioResolver::results_type resolver_results,
    const bsl::weak_ptr<AsioConnection<SocketType> >& weakConnection,
    const bsl::weak_ptr<SocketType>& weakSocket,
    const NewConnectionCallback& onSuccess,
    const ErrorCallback& onFail)
{
    bsl::shared_ptr<AsioResolver> self = weakSelf.lock();
    if (!self) {
        BALL_LOG_DEBUG << "DNS Resolution returned after resolver "
                          "destructed, ignoring";
        return;
    }

    self->handleResolve<SocketType>(host,
                                    port,
                                    error,
                                    resolver_results,
                                    weakConnection,
                                    weakSocket,
                                    onSuccess,
                                    onFail);
}

} // namespace rmqio
} // namespace BloombergLP
