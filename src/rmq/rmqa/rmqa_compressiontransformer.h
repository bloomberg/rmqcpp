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

#ifndef INCLUDED_RMQA_COMPRESSIONTRANSFORMER
#define INCLUDED_RMQA_COMPRESSIONTRANSFORMER

#include <rmqp_messagetransformer.h>
#include <rmqt_properties.h>
#include <rmqt_result.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_vector.h>
#include <bslma_managedptr.h>
#include <bsls_keyword.h>

namespace BloombergLP {
namespace rmqa {

/// \class CompressionTransformer
/// \brief A message transformer that compresses messages using zstd
///
/// CompressionTransformers are typically bound to Producers and
/// Consumers, to process messages before sending and after receiving them.
class CompressionTransformer {
  public:
    // CREATORS
    /// Create an instance of a CompressionTransformer that can be used
    /// by a single thread to manage message compression.
    /// The function may fail if insufficient memory is available to allocate
    /// a compression context.
    static rmqt::Result<rmqp::MessageTransformer> create();

    explicit CompressionTransformer(
        bslma::ManagedPtr<rmqp::MessageTransformer>& impl);

    ~CompressionTransformer();

    // MANIPULATORS
    /// Transform the given `data` and `props` in-place into compressed form,
    /// if possible. The resulting data will never be larger than the original
    /// data, and the properties will be updated to indicate that the message
    /// has been compressed.
    ///
    /// \param data  The data to be compressed
    /// \param props The message properties
    ///
    /// \return true   Returned if the data was successfully compressed
    /// \return false  Returned if the data is deemed to be not compressible.
    ///                In this case, the data and properties will remain
    ///                unchanged
    /// \return error  Returned if the compression failed for any other
    ///                reason. The data and properties will remain
    ///                unchanged
    rmqt::Result<bool> transform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                                 rmqt::Properties& props);

    /// Decompresses the given `data` and `props` in-place, if they were
    /// compressed. If the data is not compressed, it will remain unchanged.
    ///
    /// \param data  The data to be compressed
    /// \param props The message properties
    ///
    /// \return true   Returned if the data was successfully decompressed
    /// \return false  Returned if the decompression failed
    rmqt::Result<>
    inverseTransform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                     rmqt::Properties& props);

    /// Returns the header name used by this transformer to indicate
    /// that the message has been compressed.
    bsl::string name() const;

  private:
    CompressionTransformer(const CompressionTransformer&) BSLS_KEYWORD_DELETED;
    CompressionTransformer&
    operator=(const CompressionTransformer&) BSLS_KEYWORD_DELETED;

  private:
    bslma::ManagedPtr<rmqp::MessageTransformer> d_impl;
};

} // namespace rmqa
} // namespace BloombergLP

#endif
