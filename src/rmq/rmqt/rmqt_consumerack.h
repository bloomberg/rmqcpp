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

#ifndef INCLUDED_RMQT_CONSUMERACK
#define INCLUDED_RMQT_CONSUMERACK

#include <rmqt_envelope.h>

namespace BloombergLP {
namespace rmqt {

class ConsumerAck {
  public:
    enum Type { ACK, REJECT, REQUEUE };

    ConsumerAck(const Envelope& envelope, Type type);
    ~ConsumerAck();

    const Envelope& envelope() const { return d_envelope; }
    Type type() const { return d_type; }

  private:
    Envelope d_envelope;
    Type d_type;
};

} // namespace rmqt
} // namespace BloombergLP

#endif
