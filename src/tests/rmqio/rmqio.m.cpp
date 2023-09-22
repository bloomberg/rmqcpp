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

#include <ball_loggermanager.h>
#include <ball_loggermanagerconfiguration.h>
#include <ball_severity.h>
#include <ball_streamobserver.h>
#include <bslma_defaultallocatorguard.h>
#include <bslma_testallocator.h>

#include <gmock/gmock.h>

#include <bsl_iostream.h>

using namespace BloombergLP;

int main(int argc, char** argv)
{
    using namespace BloombergLP;

    bslma::TestAllocator testAllocator;
    bslma::DefaultAllocatorGuard guard(&testAllocator);

    ball::StreamObserver observer(&bsl::cout);
    ball::LoggerManagerConfiguration configuration;
    ball::LoggerManager::initSingleton(&observer, configuration);
    ball::LoggerManager::singleton().setDefaultThresholdLevels(
        ball::Severity::e_OFF,
        ball::Severity::e_TRACE,
        ball::Severity::e_OFF,
        ball::Severity::e_OFF);

    testing::InitGoogleMock(&argc, argv);

    return RUN_ALL_TESTS();
}
