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

#ifndef INCLUDED_RMQAMQPT_CONFIRMSELECT
#define INCLUDED_RMQAMQPT_CONFIRMSELECT

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bsl_cstddef.h>
#include <bsl_iostream.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Provide confirm SELECT method
///
/// This method sets the channel to use publisher acknowledgements. The client
/// can only use this method on a non-transactional channel.

class ConfirmSelect {
  private:
    bool d_noWait;

  public:
    static const rmqamqpt::Constants::AMQPMethodId METHOD_ID =
        rmqamqpt::Constants::CONFIRM_SELECT;

    ConfirmSelect();

    explicit ConfirmSelect(bool noWait);

    size_t encodedSize() const { return sizeof(uint8_t); }

    // do not send reply method
    bool noWait() const { return d_noWait; }

    static bool decode(ConfirmSelect*, const uint8_t*, bsl::size_t);

    static void encode(Writer&, const ConfirmSelect&);
};

bsl::ostream& operator<<(bsl::ostream& os, const ConfirmSelect&);

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
