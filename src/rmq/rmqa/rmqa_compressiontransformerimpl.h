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

#ifndef INCLUDED_RMQA_COMPRESSIONTRANSFORMERIMPL
#define INCLUDED_RMQA_COMPRESSIONTRANSFORMERIMPL

#include <rmqp_messagetransformer.h>
#include <rmqt_properties.h>
#include <rmqt_result.h>

#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_vector.h>
#include <bsls_keyword.h>

struct ZSTD_CCtx_s;
typedef struct ZSTD_CCtx_s ZSTD_CCtx;

namespace BloombergLP {
namespace rmqa {

class CompressionTransformerImpl : public rmqp::MessageTransformer {
  public:
    CompressionTransformerImpl();

    ~CompressionTransformerImpl() BSLS_KEYWORD_OVERRIDE;

    rmqt::Result<bool> transform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                                 rmqt::Properties& props) BSLS_KEYWORD_OVERRIDE;

    rmqt::Result<>
    inverseTransform(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                     rmqt::Properties& props) BSLS_KEYWORD_OVERRIDE;

    bsl::string name() const BSLS_KEYWORD_OVERRIDE
    {
        return bsl::string("compression");
    }

  private:
    ZSTD_CCtx* zctx;

    rmqt::Result<> decompressZstd(bsl::shared_ptr<bsl::vector<uint8_t> >& data,
                                  size_t originalSize);

    CompressionTransformerImpl(const CompressionTransformerImpl&)
        BSLS_KEYWORD_DELETED;
    CompressionTransformerImpl&
    operator=(const CompressionTransformerImpl&) BSLS_KEYWORD_DELETED;
};

} // namespace rmqa
} // namespace BloombergLP

#endif
