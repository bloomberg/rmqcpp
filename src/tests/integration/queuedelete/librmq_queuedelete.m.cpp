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

#include <rmqintegration_testparameters.h>

#include <rmqa_rabbitcontext.h>
#include <rmqa_vhost.h>
#include <rmqt_plaincredentials.h>
#include <rmqt_result.h>
#include <rmqt_simpleendpoint.h>

#include <balcl_commandline.h>
#include <ball_fileobserver.h>
#include <ball_log.h>
#include <ball_severityutil.h>
#include <ball_streamobserver.h>

#include <bsl_memory.h>

using namespace BloombergLP;
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("LIBRMQ.QUEUEDELETE")
}

int main(int argc, char* argv[])
{
    rmqintegration::TestParameters params(__FILE__);

    bool ifUnused = false;
    bool ifEmpty  = false;

    balcl::OptionInfo extraArgs[] = {
        {
            "ifUnused",
            "ifUnused",
            "If unused",
            balcl::TypeInfo(&ifUnused),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
        {
            "ifEmpty",
            "ifEmpty",
            "If empty",
            balcl::TypeInfo(&ifEmpty),
            balcl::OccurrenceInfo::e_OPTIONAL,
        },
    };

    params.addExtraArgs(extraArgs);

    if (!params.parseAndConfigure(argc, argv)) {
        return -1;
    }

    rmqa::RabbitContext rabbit;

    bsl::shared_ptr<rmqa::VHost> connection = params.createConnection(rabbit);

    connection->deleteQueue(params.queueName,
                            ifUnused ? rmqt::QueueUnused::IF_UNUSED
                                     : rmqt::QueueUnused::ALLOW_IN_USE,
                            ifEmpty ? rmqt::QueueEmpty::IF_EMPTY
                                    : rmqt::QueueEmpty::ALLOW_MSG_DELETE);

    return 0;
}
