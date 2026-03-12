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

#ifdef RMQCPP_ENABLE_COMPRESSION

#include <rmqa_compressiontransformerimpl.h>

#include <rmqp_messagetransformer.h>
#include <rmqt_properties.h>
#include <rmqt_result.h>

#include <ball_log.h>

#include <zstd.h>
#include <zstd_errors.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_vector.h>

namespace BloombergLP {
namespace rmqa {

namespace {
BALL_LOG_SET_CLASS_CATEGORY("RMQA.COMPRESSIONTRANSFORMERIMPL");
}

CompressionTransformerImpl::CompressionTransformerImpl()
: zctx(ZSTD_createCCtx())
{
    if (!zctx) {
        BALL_LOG_WARN << "Failed to allocate ZSTD context. Compression will "
                         "not be enabled.";
    }
}

CompressionTransformerImpl::~CompressionTransformerImpl()
{
    ZSTD_freeCCtx(zctx);
    zctx = NULL;
}

rmqt::Result<bool> CompressionTransformerImpl::transform(
    bsl::shared_ptr<bsl::vector<uint8_t> >& data,
    rmqt::Properties& props)
{
    if (!zctx || data->size() < 8 * 1024) {
        // Do not compress small messages
        return rmqt::Result<bool>(bsl::make_shared<bool>(false));
    }

    // Allocate space for compressed data
    bsl::shared_ptr<bsl::vector<uint8_t> > compressedData =
        bsl::make_shared<bsl::vector<uint8_t> >(
            ZSTD_compressBound(data->size()));

    // Perform the compression
    size_t compressedSize = ZSTD_compressCCtx(zctx,
                                              compressedData->data(),
                                              compressedData->size(),
                                              data->data(),
                                              data->size(),
                                              1);

    if (ZSTD_isError(compressedSize)) {
        ZSTD_ErrorCode errorCode = ZSTD_getErrorCode(compressedSize);
        if (errorCode == ZSTD_error_dstSize_tooSmall) {
            // The data is not compressible
            return rmqt::Result<bool>(bsl::make_shared<bool>(false));
        }
        return rmqt::Result<bool>(ZSTD_getErrorName(errorCode));
    }

    // Update headers
    if (!props.headers) {
        props.headers = bsl::make_shared<rmqt::FieldTable>();
    }
    props.headers->emplace("sdk.transform.compression.alg",
                           rmqt::FieldValue(bsl::string("zstd")));
    props.headers->emplace(
        "sdk.transform.compression.size",
        rmqt::FieldValue(static_cast<int64_t>(data->size())));

    // Destroy the old data and replace it with compressed data
    data = compressedData;
    data->resize(compressedSize);
    return rmqt::Result<bool>(bsl::make_shared<bool>(true));
}

rmqt::Result<> CompressionTransformerImpl::decompressZstd(
    bsl::shared_ptr<bsl::vector<uint8_t> >& data,
    size_t originalSize)
{
    bsl::shared_ptr<bsl::vector<uint8_t> > decompressedData =
        bsl::make_shared<bsl::vector<uint8_t> >(originalSize);
    size_t decompressedSize = ZSTD_decompress(
        decompressedData->data(), originalSize, data->data(), data->size());
    if (ZSTD_isError(decompressedSize) ||
        (size_t)decompressedSize != originalSize) {
        BALL_LOG_ERROR << "Decompression failed: "
                       << ZSTD_getErrorName(
                              ZSTD_getErrorCode(decompressedSize));
        return rmqt::Result<>(
            bsl::string("Decompression failed: ") +
            ZSTD_getErrorName(ZSTD_getErrorCode(decompressedSize)));
    }

    // Replace the data with decompressed data
    data = decompressedData;
    return rmqt::Result<>();
}

rmqt::Result<> CompressionTransformerImpl::inverseTransform(
    bsl::shared_ptr<bsl::vector<uint8_t> >& data,
    rmqt::Properties& props)
{
    if (!props.headers) {
        return rmqt::Result<>("Malformed message");
    }
    int64_t originalSize =
        (*props.headers)["sdk.transform.compression.size"].the<int64_t>();
    if (originalSize <= 0) {
        BALL_LOG_ERROR << "Invalid original size for decompression: "
                       << originalSize;
        return rmqt::Result<>("Invalid original size for decompression: " +
                              bsl::to_string(originalSize));
    }
    bsl::string compressionAlg =
        (*props.headers)["sdk.transform.compression.alg"].the<bsl::string>();

    rmqt::Result<> result;
    if (compressionAlg == "zstd") {
        result = decompressZstd(data, originalSize);
    }
    else {
        // Unsupported compression algorithm
        BALL_LOG_ERROR << "Unsupported compression algorithm: "
                       << compressionAlg;
        return rmqt::Result<>("Unsupported compression algorithm: " +
                              compressionAlg);
    }

    if (!result) {
        return result; // Propagate error from decompression
    }

    // Remove compression headers
    props.headers->erase("sdk.transform.compression.alg");
    props.headers->erase("sdk.transform.compression.size");
    return rmqt::Result<>();
}

} // namespace rmqa
} // namespace BloombergLP

#endif // RMQCPP_ENABLE_COMPRESSION
