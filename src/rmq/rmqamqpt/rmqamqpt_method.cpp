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

#include <rmqamqpt_method.h>

#include <rmqamqpt_types.h>

#include <ball_log.h>
#include <bdlb_bigendian.h>
#include <bsl_cstdint.h>
#include <bsl_ostream.h>

namespace BloombergLP {
namespace rmqamqpt {
namespace {
BALL_LOG_SET_NAMESPACE_CATEGORY("RMQAMQPT.METHOD")

bool decodeMethodPayload(Method* method,
                         bsl::uint16_t classId,
                         const uint8_t* data,
                         bsl::size_t dataLen)
{
#define DECODE_METHOD_PAYLOAD(MethodClass)                                     \
    case MethodClass::CLASS_ID: {                                              \
        method->createInPlace<MethodClass>();                                  \
        bool rc = MethodClass::Util::decode(                                   \
            &method->the<MethodClass>(), data, dataLen);                       \
                                                                               \
        if (!rc) {                                                             \
            BALL_LOG_ERROR << "MethodClass::decode failed [class: "            \
                           << MethodClass::CLASS_ID << "] rc: " << rc;         \
        }                                                                      \
                                                                               \
        return rc;                                                             \
    } break;

    switch (classId) {
        DECODE_METHOD_PAYLOAD(BasicMethod)
        DECODE_METHOD_PAYLOAD(ConfirmMethod)
        DECODE_METHOD_PAYLOAD(ConnectionMethod)
        DECODE_METHOD_PAYLOAD(ChannelMethod)
        DECODE_METHOD_PAYLOAD(ExchangeMethod)
        DECODE_METHOD_PAYLOAD(QueueMethod)
        default: {
            BALL_LOG_ERROR << "Failed to decode Method with class id: "
                           << classId;
        }
    }
    return false;

#undef DECODE_METHOD_PAYLOAD
}

template <class MethodClass>
bool subClassMatch(const MethodClass& lhs, const MethodClass& rhs)
{
    return lhs.methodId() == rhs.methodId();
}

struct MethodEncoder {

    MethodEncoder(Writer& os)
    : d_os(os)
    {
    }

    template <typename MethodSpec>
    void operator()(const MethodSpec& methodSpec) const
    {
        MethodSpec::Util::encode(d_os, methodSpec);
    }

    void operator()(const bslmf::Nil&) const {}

  private:
    Writer& d_os;
};

struct MethodClassIdFetcher {
    typedef uint16_t ResultType;

    template <typename T>
    ResultType operator()(const T&) const
    {
        return T::CLASS_ID;
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

rmqamqpt::Constants::AMQPClassId Method::classId() const
{
    return static_cast<rmqamqpt::Constants::AMQPClassId>(
        this->apply(MethodClassIdFetcher()));
}

size_t Method::encodedSize() const
{
    return sizeof(uint16_t) + this->apply(MethodSizeFetcher());
}

bool Method::Util::decode(Method* method,
                          const uint8_t* data,
                          bsl::size_t dataLen)
{
    bdlb::BigEndianUint16 classId;

    if (dataLen < sizeof(classId)) {
        BALL_LOG_ERROR << "Not enough data to read classId";
        return false;
    }

    memcpy(&classId, data, sizeof(classId));

    return decodeMethodPayload(
        method, classId, data + sizeof(classId), dataLen - sizeof(classId));
}

void Method::Util::encode(Writer& output, const Method& method)
{
    Types::write(output, bdlb::BigEndianUint16::make(method.classId()));

    method.apply(MethodEncoder(output));
}

bool Method::Util::typeMatch(const Method& lhs, const Method& rhs)
{
    if (&lhs == &rhs) {
        return true;
    }
    if (lhs.classId() == rhs.classId()) {
        switch (lhs.classId()) {
            case BasicMethod::CLASS_ID:
                return subClassMatch(lhs.the<BasicMethod>(),
                                     rhs.the<BasicMethod>());
            case ChannelMethod::CLASS_ID:
                return subClassMatch(lhs.the<ChannelMethod>(),
                                     rhs.the<ChannelMethod>());
            case ConnectionMethod::CLASS_ID:
                return subClassMatch(lhs.the<ConnectionMethod>(),
                                     rhs.the<ConnectionMethod>());
            case ExchangeMethod::CLASS_ID:
                return subClassMatch(lhs.the<ExchangeMethod>(),
                                     rhs.the<ExchangeMethod>());
            case QueueMethod::CLASS_ID:
                return subClassMatch(lhs.the<QueueMethod>(),
                                     rhs.the<QueueMethod>());
            default:
                break;
        }
    }
    return false;
}

} // namespace rmqamqpt
} // namespace BloombergLP
