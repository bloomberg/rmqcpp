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

// rmqt_consumerconfig.h
#ifndef INCLUDED_RMQT_CONSUMERCONFIG
#define INCLUDED_RMQT_CONSUMERCONFIG

#include <rmqt_properties.h>
#include <rmqt_queue.h>
#include <rmqt_topology.h>

#include <bdlmt_threadpool.h>

#include <bsl_cstdint.h>
#include <bsl_optional.h>

namespace BloombergLP {
namespace rmqt {

/// \brief Class for passing arguments to Consumer
///
/// This class provides passing arguments to Consumer.

class ConsumerConfig {
  public:
    static const uint16_t s_defaultPrefetchCount;

    /// \brief Util method to generate a default Consumer tag.
    static bsl::string generateConsumerTag();

    /// \param consumerTag A label for the consumer which is displayed on the
    ///        RabbitMQ Management UI. It is useful to give this a meaningful
    ///        name.
    /// \param prefetchCount Used by the RabbitMQ broker to limit the number of
    ///        messages held by a consumer at one time. Higher values can
    ///        increase throughput, particularly in high latency environments.
    /// \param threadpool threadpool which should be used to process consumer
    ///        (message) callbacks, defaults to using the context level
    ///        threadpool
    /// \param exclusiveFlag Indicate to RabbitMQ that this consumer should be
    ///        the only consumer from the specified queue. This may cause the
    ///        consumer to remain in a declaration loop if another consumer
    ///        exists on this queue.
    /// \param consumerPriority Indicate to RabbitMQ the priority of the
    /// consumer (between 0 and 10, higher is more important) the broker
    /// will send to available consumers in terms of priority provided the
    /// consumer is available and not blocked (per prefetchCount) see
    /// https://www.rabbitmq.com/consumer-priority.html#definitions for more
    /// details
    explicit ConsumerConfig(
        const bsl::string& consumerTag          = generateConsumerTag(),
        uint16_t prefetchCount                  = s_defaultPrefetchCount,
        bdlmt::ThreadPool* threadpool           = 0,
        rmqt::Exclusive::Value exclusiveFlag    = rmqt::Exclusive::OFF,
        bsl::optional<int64_t> consumerPriority = bsl::optional<int64_t>());

    ~ConsumerConfig();

    // Getters
    const bsl::string& consumerTag() const { return d_consumerTag; }

    uint16_t prefetchCount() const { return d_prefetchCount; }

    bdlmt::ThreadPool* threadpool() const { return d_threadpool; }

    rmqt::Exclusive::Value exclusiveFlag() const { return d_exclusiveFlag; }

    bsl::optional<int64_t> consumerPriority() const
    {
        return d_consumerPriority;
    }

    // Setters
    /// \param consumerTag A label for the consumer which is displayed on the
    ///        RabbitMQ Management UI. It is useful to give this a meaningful
    ///        name.
    ConsumerConfig& setConsumerTag(const bsl::string& consumerTag)
    {
        d_consumerTag = consumerTag;
        return *this;
    }

    /// \param prefetchCount Used by the RabbitMQ broker to limit the number of
    ///        messages held by a consumer at one time. Higher values can
    ///        increase throughput, particularly in high latency environments.
    ConsumerConfig& setPrefetchCount(uint16_t prefetchCount)

    {
        d_prefetchCount = prefetchCount;
        return *this;
    }

    /// \param threadpool threadpool which should be used to process consumer
    ///        (message) callbacks, defaults to using the context level
    ///        threadpool. The passed threadpool is not owned by any `rmq`
    ///        object and must live longer than this consumer exists on the
    ///        threadpool, which will be longer than the lifetime of the
    ///        Consumer object. It's recommended to keep this threadpool alive
    ///        longer longer than the Context.
    ConsumerConfig& setThreadpool(bdlmt::ThreadPool* threadpool)
    {
        d_threadpool = threadpool;
        return *this;
    }
    /// \param exclusiveFlag Indicate to RabbitMQ that this consumer should be
    /// the
    ///        only consumer from the specified queue. This may cause the
    ///        consumer to remain in a declaration loop if another consumer
    ///        exists on this queue.
    ConsumerConfig&
    setExclusiveFlag(rmqt::Exclusive::Value exclusiveFlag = rmqt::Exclusive::ON)
    {
        d_exclusiveFlag = exclusiveFlag;
        return *this;
    }

    /// \param consumerPriority Indicate to RabbitMQ the priority of the
    /// consumer: larger numbers indicate higher priority, and both positive
    /// and negative numbers can be used. The broker will send to available
    /// consumers in terms of priority provided the consumer is available and
    /// not blocked (per prefetchCount) see
    /// https://www.rabbitmq.com/consumer-priority.html#definitions for more
    /// details
    ConsumerConfig&
    setConsumerPriority(const bsl::optional<int64_t>& consumerPriority)
    {
        d_consumerPriority = consumerPriority;
        return *this;
    }

  private:
    bsl::string d_consumerTag;
    uint16_t d_prefetchCount;
    bdlmt::ThreadPool* d_threadpool;
    rmqt::Exclusive::Value d_exclusiveFlag;
    bsl::optional<int64_t> d_consumerPriority;
};

} // namespace rmqt
} // namespace BloombergLP

#endif
