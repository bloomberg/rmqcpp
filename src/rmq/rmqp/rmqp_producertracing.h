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

#ifndef INCLUDED_RMQP_PRODUCERTRACING
#define INCLUDED_RMQP_PRODUCERTRACING

#include <rmqt_properties.h>

#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {
class Endpoint;
class ConfirmResponse;
} // namespace rmqt
namespace rmqp {

/// \brief An interface for ProducerTracing class.
///
/// A ProducerTracing class implements the create method which will create a
/// Tracing Context designed to span the lifetime of the message publication
/// process, the create method will be invoked when a message is sent, the
/// context has the opportunity to override the message properties e.g. to
/// inject tracing headers before the message is sent. The context will be held
/// until the send has been acknowledged/returned from the server
class ProducerTracing {
  public:
    class Context {
      public:
        /// \brief an opportunity to add details to the context based on the
        /// servers response to the send
        /// \param confirm has a status of ACK, REJECT, RETURN
        virtual void response(const rmqt::ConfirmResponse& confirm);

        virtual ~Context();
    };

    /// \brief create a (tracing) context with relevant pieces of the producer
    /// context and tag the messageProperties with any trace info.
    /// \param messageProperties a valid pointer to a modifiable message
    /// properties object e.g. for injecting trace info into the headers.
    /// \param routingKey the routingKey used when sending.
    /// \param exchangeName the name of the exchange the current producer is
    /// sending from.
    /// \param endpoint the server endpoint details the current
    /// producer is connected to.
    virtual bsl::shared_ptr<Context> createAndTag(
        rmqt::Properties* messageProperties,
        const bsl::string& routingKey,
        const bsl::string& exchangeName,
        const bsl::shared_ptr<const rmqt::Endpoint>& endpoint) const = 0;

    virtual ~ProducerTracing();
};

} // namespace rmqp
} // namespace BloombergLP

#endif
