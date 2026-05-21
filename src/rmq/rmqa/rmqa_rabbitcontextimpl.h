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

// rmqa_rabbitcontextimpl.h
#ifndef INCLUDED_RMQA_RABBITCONTEXTIMPL
#define INCLUDED_RMQA_RABBITCONTEXTIMPL

#include <rmqa_connectionmonitor.h>
#include <rmqa_rabbitcontextoptions.h>

#include <rmqamqp_connection.h>
#include <rmqio_eventloop.h>
#include <rmqio_watchdog.h>
#include <rmqp_connection.h>
#include <rmqp_rabbitcontext.h>
#include <rmqt_endpoint.h>
#include <rmqt_future.h>
#include <rmqt_result.h>
#include <rmqt_vhostinfo.h>

#include <bdlf_bind.h>
#include <bdlmt_threadpool.h>
#include <bsls_keyword.h>
#include <bsls_timeinterval.h>

#include <bsl_memory.h>
#include <bsl_string.h>

namespace BloombergLP {
namespace rmqa {

class RabbitContextImpl : public rmqp::RabbitContext {
  public:
    RabbitContextImpl(bslma::ManagedPtr<rmqio::EventLoop> eventLoop,
                      const rmqa::RabbitContextOptions& options);

    ~RabbitContextImpl();

    bsl::shared_ptr<rmqp::Connection>
    createVHostConnection(const bsl::string& userDefinedName,
                          const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
                          const bsl::shared_ptr<rmqt::Credentials>& credentials)
        BSLS_KEYWORD_OVERRIDE;

    bsl::shared_ptr<rmqp::Connection> createVHostConnection(
        const bsl::string& userDefinedName,
        const rmqt::VHostInfo& endpoint) BSLS_KEYWORD_OVERRIDE;

    rmqt::Future<rmqp::Connection>
    createNewConnection(const bsl::string& name,
                        const bsl::shared_ptr<rmqt::Endpoint>& endpoint,
                        const bsl::shared_ptr<rmqt::Credentials>& credentials,
                        const bsl::string& suffix);

  private:
    RabbitContextImpl(const RabbitContextImpl&) BSLS_KEYWORD_DELETED;
    RabbitContextImpl& operator=(const RabbitContextImpl&) BSLS_KEYWORD_DELETED;

  private:
    static const int DEFAULT_WATCHDOG_PERIOD = 60;
    bslma::ManagedPtr<rmqio::EventLoop> d_eventLoop;
    bsl::shared_ptr<rmqio::WatchDog> d_watchDog;
    bdlmt::ThreadPool* d_threadPool;
    bslma::ManagedPtr<bdlmt::ThreadPool> d_hostedThreadPool;
    rmqt::ErrorCallback d_onError;
    rmqt::SuccessCallback d_onSuccess;
    bsl::shared_ptr<ConnectionMonitor> d_connectionMonitor;
    bslma::ManagedPtr<rmqamqp::Connection::Factory> d_connectionFactory;
    rmqt::Tunables d_tunables;
    bsl::shared_ptr<rmqp::ConsumerTracing> d_consumerTracing;
    bsl::shared_ptr<rmqp::ProducerTracing> d_producerTracing;
};

} // namespace rmqa
} // namespace BloombergLP

#endif
