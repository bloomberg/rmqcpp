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

#include <rmqt_consumerconfig.h>

#include <bdlb_guidutil.h>

namespace BloombergLP {
namespace rmqt {

const uint16_t ConsumerConfig::s_defaultPrefetchCount = 5;

ConsumerConfig::ConsumerConfig(
    const bsl::string& consumerTag /*= generateConsumerTag()*/,
    uint16_t prefetchCount /* = s_defaultPrefetchCount*/,
    bdlmt::ThreadPool* threadpool /* = 0 */,
    rmqt::Exclusive::Value exclusiveFlag /* = rmqt::Exclusive::OFF */,
    bsl::optional<int64_t> consumerPriority /* = bsl::optional<int64_t>() */)
: d_consumerTag(consumerTag)
, d_prefetchCount(prefetchCount)
, d_threadpool(threadpool)
, d_exclusiveFlag(exclusiveFlag)
, d_consumerPriority(consumerPriority)
{
}

ConsumerConfig::~ConsumerConfig() {}

bsl::string rmqt::ConsumerConfig::generateConsumerTag()
{
    return bdlb::GuidUtil::guidToString(bdlb::GuidUtil::generate());
}
} // namespace rmqt
} // namespace BloombergLP
