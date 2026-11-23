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

#ifndef INCLUDED_RMQP_PRODUCERTAGGER
#define INCLUDED_RMQP_PRODUCERTAGGER

#include <rmqp_producer.h>
#include <rmqt_properties.h>

#include <bsl_string.h>

//@PURPOSE: Decorate messages on their way out of a Producer
//
//@CLASSES:
//  rmqp::ProducerTagger: tags an outgoing message and wraps its confirmation
//                        callback

namespace BloombergLP {
namespace rmqp {

/// \brief A hook invoked by a producer once per send, to modify the message
/// about to be published and to wrap its confirmation callback with state
/// which must outlive the send. Distributed tracing is attached this way, by
/// implementing `rmqp::ProducerTracing`.
///
/// One tagger is shared by every producer on a connection and is called
/// concurrently, so implementations must be thread safe.
class ProducerTagger {
  public:
    virtual ~ProducerTagger();

    /// Called on the sending thread, before the message is queued and before
    /// any wait on the producer's outstanding confirm limit, so an
    /// implementation may read thread local state and anything it times
    /// covers that wait.
    ///
    /// \param messageProperties owned by the producer, safe to modify in
    ///        place.
    /// \param routingKey the routing key used for this send.
    /// \param exchangeName the exchange the producer publishes to.
    /// \param confirmCallback the callback supplied by the application.
    /// \return the callback to publish with, or `confirmCallback` unchanged.
    virtual rmqp::Producer::ConfirmationCallback
    tagMessage(rmqt::Properties* messageProperties,
               const bsl::string& routingKey,
               const bsl::string& exchangeName,
               const rmqp::Producer::ConfirmationCallback& confirmCallback) = 0;
};

} // namespace rmqp
} // namespace BloombergLP

#endif
