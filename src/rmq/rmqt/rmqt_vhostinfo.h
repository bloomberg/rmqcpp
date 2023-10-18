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

#ifndef INCLUDED_RMQT_VHOSTINFO
#define INCLUDED_RMQT_VHOSTINFO

#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqt {
class Endpoint;
class Credentials;

/// \class VHostInfo
/// \brief Holds the VHost endpoint and credentials, used to connect
class VHostInfo {
  public:
    // CREATORS
    /// \brief Construct a VHostInfo
    ///
    /// \param endpoint - Produces the hostname/port to connect to the
    /// broker \param credentials - Produces credentials used for
    /// authenticating with the broker
    VHostInfo(const bsl::shared_ptr<rmqt::Endpoint> endpoint,
              const bsl::shared_ptr<rmqt::Credentials> credentials);

    virtual bsl::shared_ptr<rmqt::Endpoint> endpoint() const;
    virtual bsl::shared_ptr<rmqt::Credentials> credentials() const;

  private:
    bsl::shared_ptr<rmqt::Endpoint> d_endpoint;
    bsl::shared_ptr<rmqt::Credentials> d_credentials;
}; // class VHostInfo

} // namespace rmqt
} // namespace BloombergLP

#endif // ! INCLUDED_RMQT_VHOSTINFO
