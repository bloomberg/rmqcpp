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

#include <rmqtestutil_replayframe.h>

#include <rmqio_serializedframe.h>

#include <bsl_stdexcept.h>

namespace BloombergLP {
namespace rmqtestutil {

ReplayFrame::ReplayFrame()
: d_deque()
{
}

ReplayFrame::ReplayFrame(const ReplayDeque& deque)
: d_deque(deque)
{
}

void ReplayFrame::pushInbound(
    const bsl::shared_ptr<rmqio::SerializedFrame>& frame)
{
    d_deque.push_back(bsl::make_pair(frame, INBOUND));
}

void ReplayFrame::pushOutbound(
    const bsl::shared_ptr<rmqio::SerializedFrame>& frame)
{
    d_deque.push_back(bsl::make_pair(frame, OUTBOUND));
}

bsl::size_t ReplayFrame::getLength() const { return d_deque.size(); }

bsl::size_t ReplayFrame::getLength(FrameBoundness frameBoundness) const
{
    bsl::size_t result = 0;
    for (ReplayDeque::const_iterator it = d_deque.cbegin();
         it != d_deque.cend();
         ++it) {
        if (it->second == frameBoundness) {
            ++result;
        }
    }
    return result;
}

bsl::shared_ptr<rmqio::SerializedFrame> ReplayFrame::getFrame()
{
    bsl::pair<bsl::shared_ptr<rmqio::SerializedFrame>, FrameBoundness>
        framePair;
    if (d_deque.empty()) {
        throw bsl::runtime_error("getFrame: Empty queue");
    }
    framePair = d_deque.front();
    d_deque.pop_front();

    // Check boundness of the frame
    if (framePair.second != INBOUND) {
        throw bsl::runtime_error("Expected INBOUND frame, found OUTBOUND");
    }

    return framePair.first;
}

bsl::shared_ptr<rmqio::SerializedFrame> ReplayFrame::writeFrame()
{
    bsl::pair<bsl::shared_ptr<rmqio::SerializedFrame>, FrameBoundness>
        framePair;
    if (d_deque.empty()) {
        throw bsl::runtime_error("writeFrame: Empty queue");
    }
    framePair = d_deque.front();
    d_deque.pop_front();

    // Check boundness of the frame
    if (framePair.second != OUTBOUND) {
        throw bsl::runtime_error("Expected OUTBOUND frame, found INBOUND");
    }

    return framePair.first;
}

bool ReplayFrame::isNextDirection(ReplayFrame::FrameBoundness next)
{
    // bdlcc::Deque has no easy concept of peek
    bsl::pair<bsl::shared_ptr<rmqio::SerializedFrame>, FrameBoundness>
        framePair;
    if (d_deque.empty()) {
        return false;
    }
    framePair         = d_deque.front();
    const bool result = framePair.second == next;

    return result;
}

} // namespace rmqtestutil
} // namespace BloombergLP
