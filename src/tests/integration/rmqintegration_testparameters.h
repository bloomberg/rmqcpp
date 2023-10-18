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

#ifndef INCLUDED_RMQINTEGRATION_TESTPARAMETERS
#define INCLUDED_RMQINTEGRATION_TESTPARAMETERS

#include <rmqa_rabbitcontext.h>
#include <rmqa_vhost.h>
#include <rmqt_vhostinfo.h>

#include <balcl_commandline.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqintegration {

struct TestParameters {
    bsl::string logLevel;
    bsl::string uri;
    int port;
    int tlsPort;
    bsl::string vhost;
    bsl::string user;
    bsl::string password;
    bsl::string queueName;
    bsl::string consumerTag;
    bsl::string connectionName;
    bsl::string caCert;
    bsl::string clientCert;
    bsl::string clientKey;

  public:
    explicit TestParameters(const bsl::string& moduleName);

    template <int LENGTH>
    void addExtraArgs(const balcl::OptionInfo (&specTable)[LENGTH]);

    bool parseAndConfigure(int argc, char* argv[]);

    bsl::shared_ptr<rmqa::VHost> createConnection(rmqa::RabbitContext& rabbit,
                                                  int* id = 0);

    bsl::shared_ptr<rmqt::Credentials> credentials();
    bsl::shared_ptr<rmqt::Endpoint> endpoint();

    rmqt::VHostInfo vhostInfo();

  private:
    bsl::vector<balcl::OptionInfo> d_specTable;
    int d_connectionCount;
};

template <int LENGTH>
void TestParameters::addExtraArgs(const balcl::OptionInfo (&specTable)[LENGTH])
{
    for (int i = 0; i < LENGTH; ++i) {
        d_specTable.push_back(specTable[i]);
    }
}

} // namespace rmqintegration
} // namespace BloombergLP

#endif
