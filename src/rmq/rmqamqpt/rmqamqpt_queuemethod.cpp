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

#include <rmqamqpt_queuemethod.h>

#include <rmqamqpt_types.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>

#include <bsl_cstddef.h>

namespace BloombergLP {
namespace rmqamqpt {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.QUEUEMETHOD")

template <typename T>
bool decodeMethod(QueueMethod* queueMethod,
                  const uint8_t* data,
                  bsl::size_t dataLen)
{
    queueMethod->createInPlace<T>();
    return T::decode(&queueMethod->the<T>(), data, dataLen);
}

#define DECODE_METHOD(method)                                                  \
    case method::METHOD_ID:                                                    \
        return decodeMethod<method>(queueMethod, data, dataLen);

bool decodeQueueMethodPayload(QueueMethod* queueMethod,
                              uint16_t methodId,
                              const uint8_t* data,
                              bsl::size_t dataLen)
{
    switch (methodId) {
        DECODE_METHOD(QueueDeclare)
        DECODE_METHOD(QueueDeclareOk)
        DECODE_METHOD(QueueBind)
        DECODE_METHOD(QueueBindOk)
        DECODE_METHOD(QueueUnbind)
        DECODE_METHOD(QueueUnbindOk)
        DECODE_METHOD(QueueDelete)
        DECODE_METHOD(QueueDeleteOk)
        default: {
            BALL_LOG_ERROR << "Failed to decode QueueMethod with unknown id: "
                           << methodId;

            return false;
        }
    }
}

#undef DECODE_METHOD

struct QueueMethodEncoder {

    QueueMethodEncoder(Writer& os)
    : d_os(os)
    {
    }

    template <typename T>
    void operator()(const T& method) const
    {
        Types::write(d_os, bdlb::BigEndianUint16::make(T::METHOD_ID));
        T::encode(d_os, method);
    }

    void operator()(const bslmf::Nil&) const {}

  private:
    Writer& d_os;
};
struct MethodIdFetcher {
    typedef uint16_t ResultType;

    template <typename T>
    ResultType operator()(const T&) const
    {
        return T::METHOD_ID;
    }

    ResultType operator()(const bslmf::Nil&) const { return 0; }
};

struct MethodSizeFetcher {
    typedef size_t ResultType;

    template <typename T>
    ResultType operator()(const T& methodSpec) const
    {
        return methodSpec.encodedSize();
    }

    ResultType operator()(const bslmf::Nil&) const { return 0; }
};

} // namespace

rmqamqpt::Constants::AMQPMethodId QueueMethod::methodId() const
{
    return static_cast<rmqamqpt::Constants::AMQPMethodId>(
        this->apply(MethodIdFetcher()));
}

size_t QueueMethod::encodedSize() const
{
    return sizeof(uint16_t) + this->apply(MethodSizeFetcher());
}

bool QueueMethod::Util::decode(QueueMethod* queueMethod,
                               const uint8_t* data,
                               bsl::size_t dataLength)
{
    bdlb::BigEndianUint16 methodId;

    if (dataLength < sizeof(methodId)) {

        BALL_LOG_ERROR << "Not enough data to read methodId";
        return false;
    }

    memcpy(&methodId, data, sizeof(methodId));

    return decodeQueueMethodPayload(queueMethod,
                                    methodId,
                                    data + sizeof(methodId),
                                    dataLength - sizeof(methodId));
}

void QueueMethod::Util::encode(Writer& output, const QueueMethod& queueMethod)
{
    queueMethod.apply(QueueMethodEncoder(output));
}

QueueMethod::QueueMethod() {}

} // namespace rmqamqpt
} // namespace BloombergLP
