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

#ifndef INCLUDED_RMQAMQPT_QUEUEMETHOD
#define INCLUDED_RMQAMQPT_QUEUEMETHOD

#include <rmqamqpt_queuebind.h>
#include <rmqamqpt_queuebindok.h>
#include <rmqamqpt_queuedeclare.h>
#include <rmqamqpt_queuedeclareok.h>
#include <rmqamqpt_queuedelete.h>
#include <rmqamqpt_queuedeleteok.h>
#include <rmqamqpt_queueunbind.h>
#include <rmqamqpt_queueunbindok.h>

#include <rmqamqpt_constants.h>
#include <rmqamqpt_writer.h>

#include <bdlb_variant.h>

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqamqpt {

typedef bdlb::Variant<QueueBind,
                      QueueBindOk,
                      QueueDeclare,
                      QueueDeclareOk,
                      QueueDelete,
                      QueueDeleteOk,
                      QueueUnbind,
                      QueueUnbindOk>
    SupportedQueueMethods;

/// \brief Represent an AMQP Queue class
/// https://www.rabbitmq.com/amqp-0-9-1-reference.html#class.queue

class QueueMethod : public SupportedQueueMethods {
  public:
    static const rmqamqpt::Constants::AMQPClassId CLASS_ID =
        rmqamqpt::Constants::QUEUE;

  public:
    /// Initialise internal `bdlb::Variant` with passed queueMethod
    /// This constructor is intentionally implicit.
    template <typename T>
    QueueMethod(const T& queueMethod);

    /// Construct an unset QueueMethod variant
    QueueMethod();

    /// Fetch the METHOD_ID for the method stored in the Variant. Returns zero
    /// for an unset QueueMethod.
    rmqamqpt::Constants::AMQPMethodId methodId() const;

    size_t encodedSize() const;

    /// \brief Encode/Decode a QueueMethod to/from network wire format
    class Util {
      public:
        static bool decode(QueueMethod* queueMethod,
                           const uint8_t* data,
                           bsl::size_t dataLength);
        static void encode(Writer& output, const QueueMethod& queueMethod);
    };
};

template <typename T>
QueueMethod::QueueMethod(const T& queueMethod)
: SupportedQueueMethods(queueMethod)
{
}

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
