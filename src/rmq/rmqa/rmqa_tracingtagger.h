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

#ifndef INCLUDED_RMQA_TRACINGTAGGER
#define INCLUDED_RMQA_TRACINGTAGGER

#include <rmqp_producertagger.h>

#include <rmqp_producertracing.h>
#include <rmqt_endpoint.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsls_keyword.h>

//@PURPOSE: Attach a configured rmqp::ProducerTracing to a producer
//
//@CLASSES:
//  rmqa::TracingTagger: adapts rmqp::ProducerTracing onto rmqp::ProducerTagger

namespace BloombergLP {
namespace rmqa {

/// \brief Adapts a configured `rmqp::ProducerTracing` onto the producer's
/// tagging hook, opening a tracing context per message and holding it alive
/// until the broker responds.
///
/// Holds no per-producer state, so one instance serves every producer on a
/// connection.
class TracingTagger : public rmqp::ProducerTagger {
  public:
    TracingTagger(const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
                  const bsl::shared_ptr<rmqp::ProducerTracing>& tracing);

    rmqp::Producer::ConfirmationCallback
    tagMessage(rmqt::Properties* messageProperties,
               const bsl::string& routingKey,
               const bsl::string& exchangeName,
               const rmqp::Producer::ConfirmationCallback& confirmCallback)
        BSLS_KEYWORD_OVERRIDE;

  private:
    bsl::shared_ptr<const rmqt::Endpoint> d_endpoint;
    bsl::shared_ptr<rmqp::ProducerTracing> d_tracing;

}; // class TracingTagger

} // namespace rmqa
} // namespace BloombergLP

#endif
