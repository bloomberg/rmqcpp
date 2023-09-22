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

#include <rmqio_asioresolver.h>

#include <bdlf_bind.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bsl_cstdio.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

using namespace BloombergLP;
using namespace rmqio;
using namespace ::testing;

namespace {

class ResolverTests : public Test {
  public:
    class Callbacks {
      public:
        Callbacks()
        : failCount(0)
        {
        }

        void success() {}
        void failure(const Resolver::Error&) { ++failCount; }
        void read(const rmqamqpt::Frame&) {}
        void error(Connection::ReturnCode) {}

        size_t failCount;
    };
};

} // namespace

TEST_F(ResolverTests, Breathing)
{
    AsioEventLoop loop;
    bsl::shared_ptr<AsioResolver> resolver(AsioResolver::create(loop, false));
}

TEST_F(ResolverTests, badresolve)
{
    using bdlf::PlaceHolders::_1;
    Callbacks c;

    bsl::shared_ptr<AsioResolver> resolver;

    {
        AsioEventLoop loop;
        resolver = AsioResolver::create(loop, false);

        Connection::Callbacks concb;
        concb.onRead =
            bdlf::BindUtil::bind(&Callbacks::read, &c, bdlf::PlaceHolders::_1);
        concb.onError =
            bdlf::BindUtil::bind(&Callbacks::error, &c, bdlf::PlaceHolders::_1);

        size_t maxFrameSize = 2048;
        resolver->asyncConnect(
            "this_is_an_invalid_uri",
            5672u,
            maxFrameSize,
            concb,
            bdlf::BindUtil::bind(&Callbacks::success, &c),
            bdlf::BindUtil::bind(&Callbacks::failure, &c, _1));
        loop.start();
    }
    EXPECT_THAT(c.failCount,
                Eq(1)); // either one must be called
}

TEST_F(ResolverTests, ShufflesResolverResults)
{
    bsl::string host = "host";
    bsl::string port = "port";
    typedef boost::asio::ip::basic_resolver_entry<boost::asio::ip::tcp>
        entry_type;

    bsl::vector<entry_type> entries;
    for (int i = 0; i < 5; i++) {
        bsl::string ip = bsl::to_string(i) + ".0.0.0";
        entry_type::endpoint_type endpoint(
            boost::asio::ip::address::from_string(std::string(ip)), 1);
        entries.push_back(entry_type(endpoint, host, port));
    }
    AsioResolver::results_type resolverResults =
        AsioResolver::results_type::create(
            entries.begin(), entries.end(), host, port);

    int seed = 1;
    CustomRandomGenerator g(seed);
    const bool shuffle = true;
    AsioResolver::shuffleResolverResults(
        resolverResults, shuffle, g, host, port);

    // this is contingent on seed=1 and implementation of bdlb::generate_seed()
    // Note: Only the tests care about the implementation of
    // bdlb::generate_seed()
    bsl::vector<int> expectedOrder;
    expectedOrder.push_back(4);
    expectedOrder.push_back(3);
    expectedOrder.push_back(0);
    expectedOrder.push_back(2);
    expectedOrder.push_back(1);

    int i = 0;
    for (AsioResolver::results_type::iterator it = resolverResults.begin();
         it != resolverResults.end();
         it++) {
        bsl::string expectedIp = bsl::to_string(expectedOrder[i++]) + ".0.0.0";
        EXPECT_THAT(it->endpoint().address().to_string(), Eq(expectedIp));
    }
}

TEST_F(ResolverTests, NoShuffleDoesNotReorderResolverResults)
{
    bsl::string host = "host";
    bsl::string port = "port";
    typedef boost::asio::ip::basic_resolver_entry<boost::asio::ip::tcp>
        entry_type;

    bsl::vector<entry_type> entries;
    for (int i = 0; i < 5; i++) {
        bsl::string ip = bsl::to_string(i) + ".0.0.0";
        entry_type::endpoint_type endpoint(
            boost::asio::ip::address::from_string(std::string(ip)), 1);
        entries.push_back(entry_type(endpoint, host, port));
    }
    AsioResolver::results_type resolverResults =
        AsioResolver::results_type::create(
            entries.begin(), entries.end(), host, port);

    int seed = 1;
    CustomRandomGenerator g(seed);
    const bool shuffle = false;
    AsioResolver::shuffleResolverResults(
        resolverResults, shuffle, g, host, port);

    int i = 0;
    for (AsioResolver::results_type::iterator it = resolverResults.begin();
         it != resolverResults.end();
         it++) {
        bsl::string expectedIp = bsl::to_string(i++) + ".0.0.0";
        EXPECT_THAT(it->endpoint().address().to_string(), Eq(expectedIp));
    }
}
