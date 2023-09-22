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

#ifndef INCLUDED_RMQTESTUTIL_REPLAYFRAME
#define INCLUDED_RMQTESTUTIL_REPLAYFRAME

#include <rmqio_serializedframe.h>

#include <bsl_deque.h>

#include <bsl_cstddef.h>
#include <bsl_memory.h>
#include <bsl_utility.h>

//@PURPOSE: Provides a list of ordered frames for testing
//
//@CLASSES:
//  rmqtestutil::ReplayFrame: Maintains the list of ordered frames. It can be
//  used to simulate the communication between server and client for testing
//  purpose.

namespace BloombergLP {
namespace rmqtestutil {

class ReplayFrame {
  public:
    // inbound (from server to client)
    // outbound (from client to server)
    typedef enum FrameBoundness { INBOUND = 0, OUTBOUND = 1 } FrameBoundness;
    typedef bsl::deque<
        bsl::pair<bsl::shared_ptr<rmqio::SerializedFrame>, FrameBoundness> >
        ReplayDeque;

    ReplayFrame();
    ReplayFrame(const ReplayDeque& deque);

    void pushInbound(const bsl::shared_ptr<rmqio::SerializedFrame>& frame);
    void pushOutbound(const bsl::shared_ptr<rmqio::SerializedFrame>& frame);
    bsl::size_t getLength() const;
    bsl::shared_ptr<rmqio::SerializedFrame> getFrame();
    bsl::shared_ptr<rmqio::SerializedFrame> writeFrame();
    bsl::size_t getLength(FrameBoundness) const;

    bool isNextDirection(ReplayFrame::FrameBoundness next);

  private:
    ReplayDeque d_deque;
};

} // namespace rmqtestutil
} // namespace BloombergLP

#endif
