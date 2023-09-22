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

#ifndef INCLUDED_RMQAMQPT_CONSTANTS
#define INCLUDED_RMQAMQPT_CONSTANTS

#include <bsl_cstddef.h>
#include <bsl_cstdint.h>

namespace BloombergLP {
namespace rmqamqpt {

/// \brief Represent the AMQP constants:
//  https://www.rabbitmq.com/amqp-0-9-1-reference.html#constants

class Constants {
  public:
    static const uint8_t PROTOCOL_HEADER[];
    static const bsl::size_t PROTOCOL_HEADER_LENGTH;
    static const bsl::uint8_t FRAME_END;
    static const char* AUTHENTICATION_MECHANISM;
    static const char* LOCALE;
    static const char* PLATFORM;
    static const char* PRODUCT;
    static const char* VERSION;

    // https://www.rabbitmq.com/resources/specs/amqp0-9-1.pdf (Ref 2.3.5)
    typedef enum {
        METHOD    = 1,
        HEADER    = 2,
        BODY      = 3,
        HEARTBEAT = 8
    } AMQPFrameType;

    typedef enum {
        CONNECTION = 10,
        CHANNEL    = 20,
        EXCHANGE   = 40,
        QUEUE      = 50,
        BASIC      = 60,
        CONFIRM    = 85,
        TX         = 90,

        NO_CLASS = 0
    } AMQPClassId;

    typedef enum {
        CHANNEL_OPEN         = 10, // CHANNEL
        CHANNEL_OPENOK       = 11,
        CHANNEL_FLOW         = 20,
        CHANNEL_FLOWOK       = 21,
        CHANNEL_CLOSE        = 40,
        CHANNEL_CLOSEOK      = 41,
        CONFIRM_SELECT       = 10,
        CONFIRM_SELECTOK     = 11,
        CONNECTION_START     = 10, // CONNECTION
        CONNECTION_STARTOK   = 11,
        CONNECTION_SECURE    = 20,
        CONNECTION_SECUREOK  = 21,
        CONNECTION_TUNE      = 30,
        CONNECTION_TUNEOK    = 31,
        CONNECTION_OPEN      = 40,
        CONNECTION_OPENOK    = 41,
        CONNECTION_CLOSE     = 50,
        CONNECTION_CLOSEOK   = 51,
        CONNECTION_BLOCKED   = 60,
        CONNECTION_UNBLOCKED = 61,
        BASIC_QOS            = 10, // BASIC
        BASIC_QOSOK          = 11,
        BASIC_CONSUME        = 20,
        BASIC_CONSUMEOK      = 21,
        BASIC_CANCEL         = 30,
        BASIC_CANCELOK       = 31,
        BASIC_PUBLISH        = 40,
        BASIC_RETURN         = 50,
        BASIC_DELIVER        = 60,
        BASIC_GET            = 70,
        BASIC_GETOK          = 71,
        BASIC_GETEMPTY       = 72,
        BASIC_ACK            = 80,
        BASIC_REJECT         = 90,
        BASIC_RECOVERASYNC   = 100,
        BASIC_RECOVER        = 110,
        BASIC_RECOVEROK      = 111,
        BASIC_NACK           = 120,
        EXCHANGE_DECLARE     = 10, // EXCHANGE
        EXCHANGE_DECLAREOK   = 11,
        EXCHANGE_DELETE      = 20,
        EXCHANGE_DELETEOK    = 21,
        EXCHANGE_BIND        = 30,
        EXCHANGE_BINDOK      = 31,
        EXCHANGE_UNBIND      = 40,
        EXCHANGE_UNBINDOK    = 51, // hmmm I wonder if they meant 41, too late!
        QUEUE_DECLARE        = 10, // QUEUE
        QUEUE_DECLAREOK      = 11,
        QUEUE_BIND           = 20,
        QUEUE_BINDOK         = 21,
        QUEUE_PURGE          = 30,
        QUEUE_PURGEOK        = 31,
        QUEUE_DELETE         = 40,
        QUEUE_DELETEOK       = 41,
        QUEUE_UNBIND         = 50,
        QUEUE_UNBINDOK       = 51,

        NO_METHOD = 0
    } AMQPMethodId;

    typedef enum {
        REPLY_SUCCESS       = 200,
        CONTENT_TOO_LARGE   = 311,
        NO_ROUTE            = 312,
        NO_CONSUMERS        = 313,
        CONNECTION_FORCED   = 320,
        INVALID_PATH        = 402,
        ACCESS_REFUSED      = 403,
        NOT_FOUND           = 404,
        RESOURCE_LOCKED     = 405,
        PRECONDITION_FAILED = 406,
        FRAME_ERROR         = 501,
        SYNTAX_ERROR        = 502,
        COMMAND_INVALID     = 503,
        CHANNEL_ERROR       = 504,
        UNEXPECTED_FRAME    = 505,
        RESOURCE_ERROR      = 506,
        NOT_ALLOWED         = 530,
        NOT_IMPLEMENTED     = 540,
        INTERNAL_ERROR      = 541
    } AMQPReplyCode;

    static const bsl::size_t FLOAT_OCTETS;
    static const bsl::size_t DOUBLE_OCTETS;
    static const bsl::size_t DECIMAL_OCTETS;
};

} // namespace rmqamqpt
} // namespace BloombergLP

#endif
