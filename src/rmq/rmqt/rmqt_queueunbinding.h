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

#ifndef INCLUDED_RMQT_QUEUEUNBINDING
#define INCLUDED_RMQT_QUEUEUNBINDING

#include <rmqt_binding.h>
#include <rmqt_exchange.h>
#include <rmqt_fieldvalue.h>
#include <rmqt_queue.h>

#include <bsl_ostream.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqt {

/// \brief An AMQP queue unbinding
///
/// This class represents an unbinding between an exchange and a queue to be
/// declared to the RabbitMQ broker.

class QueueUnbinding : public Binding {
  public:
    QueueUnbinding(const ExchangeHandle& exchange,
                   const QueueHandle& queue,
                   const bsl::string& bindingKey,
                   const rmqt::FieldTable& args = rmqt::FieldTable())
    : Binding(bindingKey, args)
    , d_exchange(exchange)
    , d_queue(queue)
    {
    }

    const QueueHandle& queue() const { return d_queue; }
    const ExchangeHandle& exchange() const { return d_exchange; }

    friend bsl::ostream& operator<<(bsl::ostream& os,
                                    const QueueUnbinding& queue);

  private:
    ExchangeHandle d_exchange;
    QueueHandle d_queue;
};

bsl::ostream& operator<<(bsl::ostream& os, const QueueUnbinding& queue);

} // namespace rmqt
} // namespace BloombergLP
#endif
