// Copyright 2025 Bloomberg Finance L.P.
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

#ifndef INCLUDED_RMQP_MESSAGETRANSFORMER
#define INCLUDED_RMQP_MESSAGETRANSFORMER

#include <rmqt_properties.h>
#include <rmqt_result.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqp {

/// \brief Abstract class for message transformations.
class MessageTransformer {
  public:
    virtual ~MessageTransformer() {};

    /// \brief Transform the data and properties of a message.
    ///
    /// The transform method can modify the data and properties of a message
    /// (ideally in place).
    /// It should return `true` if the transformation was successful and should
    /// be reversed upon receipt of this message, `false` if the transformation
    /// was not applicable or should not be reversed, and an error message if
    /// the transformation failed due to an error.
    virtual rmqt::Result<bool>
    transform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
              rmqt::Properties& props) = 0;

    /// \brief Inverse transform the data and properties of a message.
    ///
    /// This method should reverse the effects of the `transform` method.
    /// It should return an empty Result on success, or an error message
    /// on failure.
    virtual rmqt::Result<>
    inverseTransform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                     rmqt::Properties& props) = 0;

    virtual bsl::string name() const = 0;
};

} // namespace rmqp
} // namespace BloombergLP

#endif
