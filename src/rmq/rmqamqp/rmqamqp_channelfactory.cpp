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

namespace BloombergLP {
namespace rmqamqp {

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
    return bsl::make_shared<ReceiveChannel>(topology,
                                            onAsyncWrite,
                                            retryHandler,
                                            metricPublisher,
                                            consumerConfig,
                                            vhost,
                                            ackQueue,
                                            hungProgressTimer,
                                            connErrorCb);
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

} // namespace rmqamqp
} // namespace BloombergLP
