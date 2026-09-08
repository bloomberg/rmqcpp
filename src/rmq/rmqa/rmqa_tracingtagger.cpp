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

#include <rmqa_tracingtagger.h>

#include <rmqt_confirmresponse.h>

#include <bdlf_bind.h>
#include <bdlf_placeholder.h>

namespace BloombergLP {
namespace rmqa {

namespace {

void callbackAndContext(const rmqp::Producer::ConfirmationCallback& callback,
                        bsl::shared_ptr<rmqp::ProducerTracing::Context> context,
                        const rmqt::Message& message,
                        const bsl::string& routingKey,
                        const rmqt::ConfirmResponse& response)
{
    callback(message, routingKey, response);
    context->response(response);
}

} // namespace

TracingTagger::TracingTagger(
    const bsl::shared_ptr<const rmqt::Endpoint>& endpoint,
    const bsl::shared_ptr<rmqp::ProducerTracing>& tracing)
: d_endpoint(endpoint)
, d_tracing(tracing)
{
}

rmqp::Producer::ConfirmationCallback TracingTagger::tagMessage(
    rmqt::Properties* messageProperties,
    const bsl::string& routingKey,
    const bsl::string& exchangeName,
    const rmqp::Producer::ConfirmationCallback& confirmCallback)
{
    bsl::shared_ptr<rmqp::ProducerTracing::Context> context =
        d_tracing->createAndTag(
            messageProperties, routingKey, exchangeName, d_endpoint);

    return bdlf::BindUtil::bind(&callbackAndContext,
                                confirmCallback,
                                context,
                                bdlf::PlaceHolders::_1,
                                bdlf::PlaceHolders::_2,
                                bdlf::PlaceHolders::_3);
}

} // namespace rmqa
} // namespace BloombergLP
