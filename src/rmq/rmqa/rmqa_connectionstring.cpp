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

#include <rmqa_connectionstring.h>

#include <rmqt_plaincredentials.h>
#include <rmqt_simpleendpoint.h>
#include <rmqt_vhostinfo.h>

#include <bsl_algorithm.h>
#include <bsl_iostream.h>
#include <bsl_limits.h>
#include <bsl_memory.h>
#include <bsl_stdexcept.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqa {

bsl::optional<rmqt::VHostInfo> ConnectionString::parse(bsl::string_view uri)
{
    bsl::string_view scheme   = "";
    bsl::string_view username = "guest";
    bsl::string_view password = "guest";
    bsl::string_view hostname = "";
    bsl::string_view port     = "";
    bsl::string_view vhost    = "/";

    if (!ConnectionString::parseParts(
            &scheme, &username, &password, &hostname, &port, &vhost, uri)) {
        return bsl::optional<rmqt::VHostInfo>();
    }

    if (hostname == "") {
        return bsl::optional<rmqt::VHostInfo>();
    }

    if (port == "") {
        port = scheme == "amqp" ? "5672" : "5671";
    }

    uint16_t portNum16 = 0;
    try {
        int portNum = bsl::stoi(bsl::string(port));

        if (portNum <= 0 || portNum > bsl::numeric_limits<uint16_t>::max()) {
            return bsl::optional<rmqt::VHostInfo>();
        }

        portNum16 = static_cast<uint16_t>(portNum);
    }
    catch (const bsl::invalid_argument&) {
        return bsl::optional<rmqt::VHostInfo>();
    }
    catch (const bsl::out_of_range&) {
        return bsl::optional<rmqt::VHostInfo>();
    }

    bsl::shared_ptr<rmqt::Endpoint> endpoint =
        bsl::make_shared<rmqt::SimpleEndpoint>(hostname, vhost, portNum16);

    bsl::shared_ptr<rmqt::Credentials> credentials =
        bsl::make_shared<rmqt::PlainCredentials>(username, password);

    return rmqt::VHostInfo(endpoint, credentials);
}

bool ConnectionString::parseParts(bsl::string_view* scheme,
                                  bsl::string_view* username,
                                  bsl::string_view* password,
                                  bsl::string_view* hostname,
                                  bsl::string_view* port,
                                  bsl::string_view* vhost,
                                  bsl::string_view uri)
{
    // clang-format off
    /*
    https://www.rabbitmq.com/uri-spec.html

    amqp_URI       = "(amqp|amqps)://" amqp_authority [ "/" vhost ] [ "?" query ]
    amqp_authority = [ amqp_userinfo "@" ] host [ ":" port ]
    amqp_userinfo  = username [ ":" password ]
    username       = *( unreserved / pct-encoded / sub-delims )
    password       = *( unreserved / pct-encoded / sub-delims )
    vhost          = segment
    */
    // clang-format on

    // 1. Read the protocol. E.g. amqp://
    const size_t protocol_pos = uri.find("://");
    if (protocol_pos == bsl::string_view::npos) {
        return false;
    }
    const size_t protocol_end = protocol_pos + 2;
    *scheme                   = uri.substr(0, protocol_pos);
    if (*scheme != "amqp" && *scheme != "amqps") {
        return false;
    }
    const size_t amqp_authority_begin = protocol_pos + 3;

    if (uri.size() <= amqp_authority_begin) {
        // A valid input must contain more than just "amqp://"
        return false;
    }

    // 2. Find the vhost separator from the end, ensuring we don't just see
    // the end of 'amqp://'. Skip over if it's not there
    size_t vhost_pos = uri.rfind("/");
    if (vhost_pos == protocol_end) {
        vhost_pos = bsl::string_view::npos;
    }
    if (vhost_pos != bsl::string_view::npos) {
        *vhost = uri.substr(vhost_pos + 1);
    }

    // 3. Find the port separator, and skip over if it's not there
    size_t port_pos = uri.rfind(":", vhost_pos);

    if (port_pos == protocol_pos) {
        port_pos = bsl::string_view::npos;
    }

    if (port_pos != bsl::string_view::npos) {
        if (uri.size() <= (port_pos + 1)) {
            // E.g. "amqp://rabbit:" <- no actual port
            return false;
        }
        *port = uri.substr(port_pos + 1, vhost_pos - (port_pos + 1));
    }

    // 4. Find where the hostname starts. Either after the @, or after the
    // protocol
    const size_t hostname_pos = uri.rfind("@", vhost_pos);
    const size_t hostname_end = bsl::min(port_pos, vhost_pos);

    if (hostname_pos == bsl::string_view::npos) {
        if (hostname_end == bsl::string_view::npos) {
            *hostname = uri.substr(amqp_authority_begin);
        }
        else {
            *hostname = uri.substr(amqp_authority_begin,
                                   hostname_end - amqp_authority_begin);
        }
    }
    else {
        // 5. An @ was present, read the username, and optionally the
        // password
        *hostname =
            uri.substr(hostname_pos + 1, hostname_end - (hostname_pos + 1));

        const size_t userPassSplit = uri.find(":", amqp_authority_begin);

        if (userPassSplit == bsl::string_view::npos) {
            // default password
            *username = uri.substr(amqp_authority_begin,
                                   hostname_pos - amqp_authority_begin);
        }
        else {
            *username = uri.substr(amqp_authority_begin,
                                   userPassSplit - amqp_authority_begin);
            *password = uri.substr(userPassSplit + 1,
                                   hostname_pos - (userPassSplit + 1));
        }
    }

    return true;
}

} // namespace rmqa
} // namespace BloombergLP
