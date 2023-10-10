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

#ifndef INCLUDED_RMQA_TRACINGPRODUCERIMPL
#define INCLUDED_RMQA_TRACINGPRODUCERIMPL

#include <rmqa_producerimpl.h>

#include <rmqp_producertracing.h>

#include <bsl_memory.h>
#include <bsls_keyword.h>

//@PURPOSE: Implements the rmqa::Producer interface and specialises for tracing
//
//@CLASSES:
//  rmqa::ProducerImpl: Manages interaction between rmqa <-> rmq internals

namespace BloombergLP {
namespace rmqa {

class TracingProducerImpl : public ProducerImpl {
  public:
    class Factory : public ProducerImpl::Factory {
      public:
        Factory(const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
                const bsl::shared_ptr<rmqp::ProducerTracing>& tracing);

        virtual bsl::shared_ptr<ProducerImpl>
        create(uint16_t maxOutstandingConfirms,
               const rmqt::ExchangeHandle& exchange,
               const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
               bdlmt::ThreadPool& threadPool,
               rmqio::EventLoop& eventLoop) const BSLS_KEYWORD_OVERRIDE;

      private:
        bsl::shared_ptr<const rmqt::Endpoint> d_endpoint;
        bsl::shared_ptr<rmqp::ProducerTracing> d_tracing;
    };

    // CREATORS
    TracingProducerImpl(uint16_t maxOutstandingConfirms,
                        const bsl::shared_ptr<rmqamqp::SendChannel>& channel,
                        bdlmt::ThreadPool& threadPool,
                        rmqio::EventLoop& eventLoop,
                        const bsl::string& exchangeName,
                        const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
                        const bsl::shared_ptr<rmqp::ProducerTracing>& tracing);

    SendStatus send(const rmqt::Message& message,
                    const bsl::string& routingKey,
                    const rmqp::Producer::ConfirmationCallback& confirmCallback,
                    const bsls::TimeInterval& timeout) BSLS_KEYWORD_OVERRIDE;

    SendStatus
    trySend(const rmqt::Message& message,
            const bsl::string& routingKey,
            const rmqp::Producer::ConfirmationCallback& confirmCallback)
        BSLS_KEYWORD_OVERRIDE;

  private:
    bsl::string d_exchangeName;
    bsl::shared_ptr<const rmqt::Endpoint> d_endpoint;
    bsl::shared_ptr<rmqp::ProducerTracing> d_tracing;

}; // class TracingProducerImpl

} // namespace rmqa
} // namespace BloombergLP

#endif
