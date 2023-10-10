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

#ifndef INCLUDED_RMQT_QUEUE
#define INCLUDED_RMQT_QUEUE

#include <rmqt_fieldvalue.h>

#include <bsl_memory.h>
#include <bsl_ostream.h>

//@PURPOSE: Provide an AMQP Queue.
//
//@CLASSES:
//  rmqt::Queue: a RabbitMQ Queue API object

namespace BloombergLP {
namespace rmqt {

/// \brief An AMQP Queue
///
/// Represents a Queue object to be declared to the RabbitMQ broker.

class Queue {
  public:
    // CREATORS
    /// Create a queue.
    /// If the name is empty, then the queue name
    /// will be auto-generated.
    explicit Queue(const bsl::string& name      = bsl::string(),
                   bool passive                 = false,
                   bool autoDelete              = false,
                   bool durable                 = true,
                   const rmqt::FieldTable& args = rmqt::FieldTable());

    /// \brief Queue name.
    const bsl::string& name() const { return d_name; }

    /// \brief Returns whether the queue is passive.
    bool passive() const { return d_passive; }

    /// \brief Returns whether the queue is "auto-delete".
    bool autoDelete() const { return d_autoDelete; }

    /// \brief Returns whether the queue is durable.
    bool durable() const { return d_durable; }

    /// \brief Optional queue arguments.
    const rmqt::FieldTable& arguments() const { return d_args; }

    friend bsl::ostream& operator<<(bsl::ostream& os, const Queue& queue);

  private:
    // DATA
    bsl::string d_name;
    bool d_passive;
    bool d_autoDelete;
    bool d_durable;
    rmqt::FieldTable d_args;
}; // class Queue

typedef bsl::weak_ptr<Queue> QueueHandle;

bsl::ostream& operator<<(bsl::ostream& os, const Queue& queue);

} // namespace rmqt
} // namespace BloombergLP

#endif // ! INCLUDED_RMQT_QUEUE
