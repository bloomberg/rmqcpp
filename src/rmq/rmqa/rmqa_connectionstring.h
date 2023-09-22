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

#ifndef INCLUDED_RMQA_CONNECTIONMONITOR
#define INCLUDED_RMQA_CONNECTIONMONITOR

#include <rmqt_vhostinfo.h>

#include <bsl_optional.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqa {

/// \class ConnectionString
/// \brief A very basic amqp/amqps connection string -> VHostInfo parser.
///
/// This does not fully implement rfc 3986. It is provided to parse straight
/// forward connection strings such as
/// 'amqp://username:password@plainhostname:5672/vhostname' and does not
/// implement proper escape patterns etc.
class ConnectionString {
  public:
    static bsl::optional<rmqt::VHostInfo> parse(bsl::string_view uri);

    static bool parseParts(bsl::string_view* scheme,
                           bsl::string_view* username,
                           bsl::string_view* password,
                           bsl::string_view* hostname,
                           bsl::string_view* port,
                           bsl::string_view* vhost,
                           bsl::string_view uri);
};

} // namespace rmqa
} // namespace BloombergLP

#endif
