// Copyright 2025 Bloomberg Finance L.P.
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

#include <bsl_memory.h>
#include <bsl_vector.h>
#include <bslma_managedptr.h>
#include <rmqa_compressiontransformer.h>
#include <rmqa_compressiontransformerimpl.h>
#include <rmqp_messagetransformer.h>
#include <rmqt_properties.h>

namespace BloombergLP {
namespace rmqa {

rmqt::Result<rmqp::MessageTransformer> CompressionTransformer::create()
{
    return rmqt::Result<rmqp::MessageTransformer>(
        bsl::make_shared<CompressionTransformerImpl>());
}

CompressionTransformer::CompressionTransformer(
    bslma::ManagedPtr<rmqp::MessageTransformer>& impl)
: d_impl(impl)
{
}

CompressionTransformer::~CompressionTransformer() {}

rmqt::Result<bool>
CompressionTransformer::transform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                                  rmqt::Properties& props)
{
    return d_impl->transform(data, props);
}

rmqt::Result<> CompressionTransformer::inverseTransform(
    bsl::shared_ptr<bsl::vector<uint8_t> >& data,
    rmqt::Properties& props)
{
    return d_impl->inverseTransform(data, props);
}

bsl::string CompressionTransformer::name() const { return d_impl->name(); }

} // namespace rmqa
} // namespace BloombergLP
