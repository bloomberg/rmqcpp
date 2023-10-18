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

// rmqio_resolver.h
#ifndef INCLUDED_RMQIO_RESOLVER_H
#define INCLUDED_RMQIO_RESOLVER_H

//@PURPOSE: Resolver interface for tcp socket
//
//@CLASSES:
// Resolver
//
//@AUTHOR: whoy1
//

// Includes
#include <rmqio_connection.h>

#include <rmqt_result.h>
#include <rmqt_securityparameters.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_string.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_ostream.h>

namespace BloombergLP {

namespace rmqt {
class SecurityParameters;
}
namespace rmqio {

//=======
// Class Resolver
//=======

class Resolver {
  public:
    enum Error { ERROR_RESOLVE = 1, ERROR_CONNECT = 2, ERROR_HANDSHAKE = 3 };

    typedef bsl::function<void()> NewConnectionCallback;
    typedef bsl::function<void(Error)> ErrorCallback;

    virtual bsl::shared_ptr<Connection>
    asyncConnect(const bsl::string& host,
                 bsl::uint16_t port,
                 bsl::size_t maxFrameSize,
                 const Connection::Callbacks& connCallbacks,
                 const NewConnectionCallback& onSuccess,
                 const ErrorCallback& onFail) = 0;

    virtual bsl::shared_ptr<Connection> asyncSecureConnect(
        const bsl::string& host,
        bsl::uint16_t port,
        bsl::size_t maxFrameSize,
        const bsl::shared_ptr<rmqt::SecurityParameters>& securityParameters,
        const Connection::Callbacks& connCallbacks,
        const NewConnectionCallback& onSuccess,
        const ErrorCallback& onFail) = 0;

    virtual ~Resolver() {}
};

bsl::ostream& operator<<(bsl::ostream&, Resolver::Error err);

} // namespace rmqio
} // namespace BloombergLP
#endif
