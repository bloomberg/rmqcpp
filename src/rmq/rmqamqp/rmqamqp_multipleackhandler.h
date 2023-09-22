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

#ifndef INCLUDED_RMQAMQP_MULTIPLEACKHANDLER
#define INCLUDED_RMQAMQP_MULTIPLEACKHANDLER

#include <rmqt_consumerack.h>

#include <bsl_algorithm.h>
#include <bsl_functional.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {

//@PURPOSE: Combine sequences of consumer acknowledgements into
//          multi-acknowledgements
//
//@CLASSES:
//  rmqamqp::MultipleAckHandler: Batches acks into multi-acks

class MultipleAckHandler {
  public:
    typedef bsl::function<void(uint64_t, bool)> AckCallback;
    typedef bsl::function<void(uint64_t, bool, bool)> NackCallback;

    MultipleAckHandler(const AckCallback& ackCb, const NackCallback& nackCb);
    ~MultipleAckHandler();

    /// Batches and acknowledges the given acks.
    /// NB: this method takes a vector by reference and modifies (sorts) it.
    void process(bsl::vector<rmqt::ConsumerAck>& acks);

    /// Reset upon channel reopen.
    void reset();

  private:
    void sendAck(uint64_t deliveryTag,
                 rmqt::ConsumerAck::Type type,
                 size_t batchSize);

    /// Callback for sending basic.ack to the broker
    AckCallback d_onAck;

    /// Callback for sending basic.nack to the broker
    NackCallback d_onNack;

    /// Total number of messages acknowledged
    uint64_t d_totalAcked;

    /// Highest delivery tag acknowledged
    uint64_t d_maxProcessedTag;
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
