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

#ifndef INCLUDED_RMQA_TRACINGCONSUMERIMPL
#define INCLUDED_RMQA_TRACINGCONSUMERIMPL

#include <rmqa_consumerimpl.h>

#include <bsl_memory.h>
#include <bsls_keyword.h>

//@PURPOSE: Provide a RabbitMQ Async Tracing Consumer API
//
//@CLASSES:
//  rmqa::Consumer: a RabbitMQ Async Tracing Consumer object

namespace BloombergLP {

namespace rmqt {
class Endpoint;
}
namespace rmqamqp {
class ReceiveChannel;
}
namespace rmqp {
class ConsumerTracing;
}

namespace rmqa {

class TracingConsumerImpl {
  public:
    class Factory : public rmqa::ConsumerImpl::Factory {
      public:
        Factory(const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
                const bsl::shared_ptr<rmqp::ConsumerTracing>& tracing);

        virtual bsl::shared_ptr<ConsumerImpl> create(
            const bsl::shared_ptr<rmqamqp::ReceiveChannel>& channel,
            rmqt::QueueHandle queue,
            const bsl::shared_ptr<rmqa::ConsumerImpl::ConsumerFunc>& onMessage,
            const bsl::string& consumerTag,
            bdlmt::ThreadPool& threadPool,
            rmqio::EventLoop& eventLoop,
            const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue) const
            BSLS_KEYWORD_OVERRIDE;

      private:
        bsl::shared_ptr<rmqt::Endpoint> d_endpoint;
        bsl::shared_ptr<rmqp::ConsumerTracing> d_tracing;
    };

}; // class TracingConsumerImpl

} // namespace rmqa
} // namespace BloombergLP

#endif // ! INCLUDED_RMQA_CONSUMERIMPL
