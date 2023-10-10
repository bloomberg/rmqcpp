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

#ifndef INCLUDED_RMQP_CONSUMERTRACING
#define INCLUDED_RMQP_CONSUMERTRACING

#include <rmqt_endpoint.h>

#include <bsl_memory.h>

namespace BloombergLP {
namespace rmqp {

class MessageGuard;

/// \brief An interface for ConsumerTracing class.
///
/// A ConsumerTracing class implements the create method which will create a
/// Tracing Context designed to span the lifetime of the message consumption
/// process, the create method will be invoked from the same thread that the
/// consumer is called back with. The context will be held until the message has
/// been 'processed' ack/nack'd
class ConsumerTracing {
  public:
    class Context {
      public:
        virtual ~Context();
    };

    /// \brief create a (tracing) context with relevant pieces of the consumer
    /// context.
    /// \param messageGuard read only reference to the messageGuard
    /// (provides access to the message/properties).
    /// \param queueName the name of the queue the current consumer is consuming
    /// from.
    /// \param endpoint the server endpoint details the current consumer is
    /// connected to.
    virtual bsl::shared_ptr<Context>
    create(const rmqp::MessageGuard& messageGuard,
           const bsl::string& queueName,
           const bsl::shared_ptr<const rmqt::Endpoint>& endpoint) const = 0;

    virtual ~ConsumerTracing();
};

} // namespace rmqp
} // namespace BloombergLP

#endif
