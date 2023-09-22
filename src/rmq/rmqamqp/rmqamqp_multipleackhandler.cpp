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

#include <rmqamqp_multipleackhandler.h>

#include <ball_log.h>

namespace BloombergLP {
namespace rmqamqp {

namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQP.MULTIPLEACKHANDLER")

bool compare(const rmqt::ConsumerAck& a, const rmqt::ConsumerAck& b)
{
    return a.envelope().deliveryTag() < b.envelope().deliveryTag();
}
} // namespace

MultipleAckHandler::MultipleAckHandler(const AckCallback& ackCb,
                                       const NackCallback& nackCb)
: d_onAck(ackCb)
, d_onNack(nackCb)
, d_totalAcked(0)
, d_maxProcessedTag(0)
{
}

MultipleAckHandler::~MultipleAckHandler() {}

void MultipleAckHandler::process(bsl::vector<rmqt::ConsumerAck>& acks)
{
    // Process the acks in increasing order of delivery tags
    bsl::sort(acks.begin(), acks.end(), compare);

    // Maintain information about pending batches -- length, last (highest) tag
    // and type
    size_t batchLength                = 0;
    uint64_t batchMaxTag              = 0;
    rmqt::ConsumerAck::Type batchType = rmqt::ConsumerAck::ACK;

    for (size_t i = 0; i < acks.size(); i++) {
        const uint64_t tag                 = acks[i].envelope().deliveryTag();
        const rmqt::ConsumerAck::Type type = acks[i].type();

        // We can only batch the next ack if we have already processed all
        // messages in the interval [1 .. n] and the next message has the tag
        // n+1
        if (d_totalAcked == d_maxProcessedTag && tag == d_maxProcessedTag + 1) {

            // If this ack has a different type than the pending batch, the
            // batch is closed and can be acked to the broker
            if (batchLength > 0 && type != batchType) {
                sendAck(batchMaxTag, batchType, batchLength);
                batchLength = 0;
            }

            // This ack starts or extends the current batch
            batchLength++;
            batchMaxTag = tag;
            batchType   = type;
        }
        else {
            // We cannot batch this message

            // If there is a pending batch, close and ack it
            if (batchLength > 0) {
                sendAck(batchMaxTag, batchType, batchLength);
                batchLength = 0;
                batchMaxTag = 0;
            }

            // Individually send this ack to the broker
            sendAck(tag, type, 1);
        }

        d_maxProcessedTag = bsl::max(d_maxProcessedTag, tag);
        d_totalAcked++;
    }

    if (batchLength > 0) {
        sendAck(batchMaxTag, batchType, batchLength);
    }
}

void MultipleAckHandler::reset()
{
    d_totalAcked      = 0;
    d_maxProcessedTag = 0;
}

void MultipleAckHandler::sendAck(uint64_t deliveryTag,
                                 rmqt::ConsumerAck::Type type,
                                 size_t batchSize)
{
    const bool multiple = (batchSize > 1);

    BALL_LOG_TRACE << (type == rmqt::ConsumerAck::ACK ? "Acking: "
                                                      : "Nacking: ")
                   << deliveryTag << ", type = " << type
                   << ", batch size = " << batchSize;

    if (type == rmqt::ConsumerAck::ACK) {
        d_onAck(deliveryTag, multiple);
    }
    else {
        d_onNack(deliveryTag, type == rmqt::ConsumerAck::REQUEUE, multiple);
    }
}

} // namespace rmqamqp
} // namespace BloombergLP
