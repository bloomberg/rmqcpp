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

#ifndef INCLUDED_RMQIO_CONNECTION
#define INCLUDED_RMQIO_CONNECTION

#include <rmqio_serializedframe.h>
#include <rmqt_result.h>

#include <bsl_functional.h>
#include <bsl_memory.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqio {

class Connection {
  public:
    enum ReturnCode {
        SUCCESS = 0,
        CONNECT_ERROR,
        GRACEFUL_DISCONNECT,
        DISCONNECTED_ERROR,
        FRAME_ERROR,
        HUNG_ERROR
    };

    typedef bsl::function<void(const rmqamqpt::Frame&)> ConsumerCallback;
    typedef bsl::function<void(ReturnCode)> ErrorCallback;
    typedef bsl::function<void(ReturnCode)> DoneCallback;
    typedef bsl::function<void()> SuccessWriteCallback;

    struct Callbacks {
        Callbacks()
        : onRead()
        , onError()
        {
        }
        ConsumerCallback onRead;
        ErrorCallback onError;
    };

    /// calls callback on success, calls registered errorcallback on failure
    virtual void
    asyncWrite(const bsl::vector<bsl::shared_ptr<SerializedFrame> >& frames,
               const SuccessWriteCallback& callback) = 0;

    /// shuts down read and write operations on the socket, will cancel
    /// outstanding reads and writes, registered error callback will be called
    /// for cancelled writes, and the passed in callback will be called once
    /// the read callback is terminated
    virtual void close(const DoneCallback& callback) = 0;

    /// returns if the socket is connected
    virtual bool isConnected() const = 0;

    virtual ~Connection() {}
};
} // namespace rmqio
} // namespace BloombergLP
#endif
