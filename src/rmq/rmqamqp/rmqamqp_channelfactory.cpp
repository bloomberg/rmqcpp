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

#include <rmqamqp_channelfactory.h>

#include <rmqamqp_metrics.h>

#include <bsl_utility.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqp {

ChannelFactory::ChannelFactory(
    const ChannelOnOpenState receiveChannelOnOpenState)
: d_receiveChannelOnOpenState(receiveChannelOnOpenState)
{
}

bsl::shared_ptr<ReceiveChannel> ChannelFactory::createReceiveChannel(
    const rmqt::Topology& topology,
    const Channel::AsyncWriteCallback& onAsyncWrite,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
    const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
    const rmqt::ConsumerConfig& consumerConfig,
    const bsl::string& vhost,
    const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
    const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
    const Channel::HungChannelCallback& connErrorCb)
{
    const bool channelPausedOnOpen =
        d_receiveChannelOnOpenState == PAUSED ||
        (consumerConfig.consumeOnlyFromHealthyHost() &&
         d_receiveChannelOnOpenState == PAUSED_HOST_HEALTH_AWARE);

    // Publish health-aware or health-unaware consumer created counter
    bsl::vector<bsl::pair<bsl::string, bsl::string> > vhostTags;
    vhostTags.push_back(
        bsl::pair<bsl::string, bsl::string>(Metrics::VHOST_TAG, vhost));

    if (consumerConfig.consumeOnlyFromHealthyHost()) {
        metricPublisher->publishCounter(
            Metrics::HEALTH_AWARE_CONSUMER_CREATED, 1.0, vhostTags);
    }
    else {
        metricPublisher->publishCounter(
            Metrics::HEALTH_UNAWARE_CONSUMER_CREATED, 1.0, vhostTags);
    }

    return bsl::make_shared<ReceiveChannel>(topology,
                                            onAsyncWrite,
                                            retryHandler,
                                            metricPublisher,
                                            consumerConfig,
                                            vhost,
                                            ackQueue,
                                            hungProgressTimer,
                                            connErrorCb,
                                            channelPausedOnOpen);
}

bsl::shared_ptr<SendChannel> ChannelFactory::createSendChannel(
    const rmqt::Topology& topology,
    const bsl::shared_ptr<rmqt::Exchange>& exchange,
    const Channel::AsyncWriteCallback& onAsyncWrite,
    const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
    const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
    const bsl::string& vhost,
    const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
    const Channel::HungChannelCallback& connErrorCb)
{
    return bsl::make_shared<SendChannel>(topology,
                                         exchange,
                                         onAsyncWrite,
                                         retryHandler,
                                         metricPublisher,
                                         vhost,
                                         hungProgressTimer,
                                         connErrorCb);
}

void ChannelFactory::setReceiveChannelOnOpenState(
    const ChannelOnOpenState channelOnOpenState)
{
    d_receiveChannelOnOpenState = channelOnOpenState;
}

} // namespace rmqamqp
} // namespace BloombergLP
