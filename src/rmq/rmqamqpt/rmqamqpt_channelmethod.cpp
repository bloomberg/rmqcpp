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

#include <rmqamqpt_channelmethod.h>

#include <rmqamqpt_types.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>
#include <bsl_cstddef.h>

namespace BloombergLP {
namespace rmqamqpt {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.CHANNELMETHOD")

template <typename T>
bool decodeMethod(ChannelMethod* channelMethod,
                  const uint8_t* data,
                  bsl::size_t dataLen)
{
    channelMethod->createInPlace<T>();
    return T::decode(&channelMethod->the<T>(), data, dataLen);
}

#define DECODE_METHOD(method)                                                  \
    case method::METHOD_ID:                                                    \
        return decodeMethod<method>(channelMethod, data, dataLen);

bool decodeChannelMethodPayload(ChannelMethod* channelMethod,
                                uint16_t methodId,
                                const uint8_t* data,
                                bsl::size_t dataLen)
{
    switch (methodId) {
        DECODE_METHOD(ChannelOpen)
        DECODE_METHOD(ChannelOpenOk)
        DECODE_METHOD(ChannelFlow)
        DECODE_METHOD(ChannelFlowOk)
        DECODE_METHOD(ChannelClose)
        DECODE_METHOD(ChannelCloseOk)
        default: {
            BALL_LOG_ERROR << "Failed to decode ChannelMethod with unknown id: "
                           << methodId;

            return false;
        }
    }
}

#undef DECODE_METHOD

struct ChannelMethodEncoder {
    ChannelMethodEncoder(Writer& os)
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

rmqamqpt::Constants::AMQPMethodId ChannelMethod::methodId() const
{
    return static_cast<rmqamqpt::Constants::AMQPMethodId>(
        this->apply(MethodIdFetcher()));
}

size_t ChannelMethod::encodedSize() const
{
    return sizeof(uint16_t) + this->apply(MethodSizeFetcher());
}

bool ChannelMethod::Util::decode(ChannelMethod* channelMethod,
                                 const uint8_t* data,
                                 bsl::size_t dataLength)
{
    bdlb::BigEndianUint16 methodId;

    if (dataLength < sizeof(methodId)) {

        BALL_LOG_ERROR << "Not enough data to read methodId";
        return false;
    }

    memcpy(&methodId, data, sizeof(methodId));

    return decodeChannelMethodPayload(channelMethod,
                                      methodId,
                                      data + sizeof(methodId),
                                      dataLength - sizeof(methodId));
}

void ChannelMethod::Util::encode(Writer& output,
                                 const ChannelMethod& channelMethod)
{
    channelMethod.apply(ChannelMethodEncoder(output));
}

ChannelMethod::ChannelMethod() {}

} // namespace rmqamqpt
} // namespace BloombergLP
