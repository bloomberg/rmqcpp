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

#ifndef INCLUDED_RMQAMQP_CHANNELFACTORY
#define INCLUDED_RMQAMQP_CHANNELFACTORY

#include <rmqamqp_receivechannel.h>
#include <rmqamqp_sendchannel.h>

#include <rmqamqpt_constants.h>
#include <rmqio_retryhandler.h>
#include <rmqt_consumerackbatch.h>
#include <rmqt_consumerconfig.h>

#include <bsl_memory.h>
#include <bsl_string.h>

//@PURPOSE: Create Channels
//
//@CLASSES: ChannelFactory

namespace BloombergLP {
namespace rmqamqp {

class ChannelFactory {
  public:
    virtual ~ChannelFactory(){};

    virtual bsl::shared_ptr<ReceiveChannel> createReceiveChannel(
        const rmqt::Topology& topology,
        const Channel::AsyncWriteCallback& onAsyncWrite,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
        const rmqt::ConsumerConfig& consumerConfig,
        const bsl::string& vhost,
        const bsl::shared_ptr<rmqt::ConsumerAckQueue>& ackQueue,
        const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
        const Channel::HungChannelCallback& connErrorCb);

    virtual bsl::shared_ptr<SendChannel> createSendChannel(
        const rmqt::Topology& topology,
        const bsl::shared_ptr<rmqt::Exchange>& exchange,
        const Channel::AsyncWriteCallback& onAsyncWrite,
        const bsl::shared_ptr<rmqio::RetryHandler>& retryHandler,
        const bsl::shared_ptr<rmqp::MetricPublisher>& metricPublisher,
        const bsl::string& vhost,
        const bsl::shared_ptr<rmqio::Timer>& hungProgressTimer,
        const Channel::HungChannelCallback& connErrorCb);
};

} // namespace rmqamqp
} // namespace BloombergLP

#endif
