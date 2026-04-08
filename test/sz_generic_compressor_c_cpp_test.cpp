#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "SZ3/utils/Config.hpp"
#include "SZ3/utils/BlockwiseIterator.hpp"
#include "SZ3/compressor/SZGenericCompressor.hpp"
#include "SZ3/compressor/SZGenericCompressor_c.h"
#include "SZ3/decomposition/InterpolationDecomposition.hpp"
#include "SZ3/decomposition/InterpolationDecomposition_c.h"
#include "SZ3/encoder/HuffmanEncoder.hpp"
#include "SZ3/encoder/HuffmanEncoder_i32_c.h"
#include "SZ3/lossless/Lossless_zstd.hpp"
#include "SZ3/quantizer/LinearQuantizer.hpp"
#include "SZ3/quantizer/line_quantizer_c.h"

namespace {

// C 侧分解器上下文：持有配置与具体实现对象。
struct CDecompositionCtx {
    SZ3InterpConfigC conf{};
    SZ3InterpolationDecompositionC decomp{};
};

// C 侧编码器上下文：持有 i32 Huffman 编码器。
struct CEncoderCtx {
    SZ3HuffmanEncoderI32 encoder{};
};

// C 侧无损上下文：直接复用 C++ 的 Lossless_zstd。
struct CLosslessCtx {
    SZ3::Lossless_zstd lossless{};
};

void print_first_64_float_values(const char *name, const float *values, size_t size) {
    const size_t count = std::min<size_t>(64, size);
    std::cout << name << " (first " << count << " values):" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    for (size_t i = 0; i < count; i++) {
        std::cout << values[i];
        if ((i + 1) % 16 == 0 || i + 1 == count) {
            std::cout << std::endl;
        } else {
            std::cout << ", ";
        }
    }
}

int fail(const std::string &msg) {
    std::cerr << "[FAIL] " << msg << std::endl;
    return 1;
}

bool close_enough(float a, float b, float eps = 1e-6f) {
    return std::fabs(a - b) <= eps;
}

// --------------------- C GenericCompressor 回调适配 ---------------------
int c_decomp_compress(void *ctx, const SZ3_Config_C *, void *data, int **quant_inds, size_t *quant_size) {
    auto *dctx = static_cast<CDecompositionCtx *>(ctx);
    *quant_inds = sz3_interp_decomp_compress(&dctx->decomp, &dctx->conf, static_cast<float *>(data), quant_size);
    return (*quant_inds == nullptr) ? -1 : 0;
}

void c_decomp_get_out_range(void *ctx, int *out_begin, int *out_end) {
    auto *dctx = static_cast<CDecompositionCtx *>(ctx);
    const auto r = sz3_interp_decomp_get_out_range(&dctx->decomp);
    *out_begin = r.min;
    *out_end = r.max;
}

size_t c_decomp_size_est(void *ctx) {
    auto *dctx = static_cast<CDecompositionCtx *>(ctx);
    return 0; // C 版本暂未暴露 size_est 接口，这里返回 0。
}

int c_decomp_save(void *ctx, unsigned char **buffer_pos) {
    auto *dctx = static_cast<CDecompositionCtx *>(ctx);
    sz3_interp_decomp_save(&dctx->decomp, buffer_pos);
    return 0;
}

int c_decomp_load(void *ctx, const unsigned char **buffer_pos, size_t buffer_size) {
    auto *dctx = static_cast<CDecompositionCtx *>(ctx);
    size_t remaining = buffer_size;
    sz3_interp_decomp_load(&dctx->decomp, buffer_pos, &remaining);
    return 0;
}

int c_decomp_decompress(void *ctx, const SZ3_Config_C *, const int *quant_inds, size_t quant_size, void *dec_data) {
    auto *dctx = static_cast<CDecompositionCtx *>(ctx);
    return sz3_interp_decomp_decompress(&dctx->decomp, &dctx->conf, const_cast<int *>(quant_inds), static_cast<float *>(dec_data), quant_size)
                   ? 0
                   : -1;
}

int c_encoder_preprocess_encode(void *ctx, const int *quant_inds, size_t quant_size, int out_range_end) {
    auto *ectx = static_cast<CEncoderCtx *>(ctx);
    return sz3_huffman_encoder_i32_preprocess_encode(&ectx->encoder, quant_inds, quant_size, out_range_end);
}

size_t c_encoder_size_est(void *ctx) {
    auto *ectx = static_cast<CEncoderCtx *>(ctx);
    return sz3_huffman_encoder_i32_size_est(&ectx->encoder);
}

int c_encoder_save(void *ctx, unsigned char **buffer_pos) {
    auto *ectx = static_cast<CEncoderCtx *>(ctx);
    sz3_huffman_encoder_i32_save(&ectx->encoder, buffer_pos);
    return 0;
}

int c_encoder_encode(void *ctx, const int *quant_inds, size_t quant_size, unsigned char **buffer_pos) {
    auto *ectx = static_cast<CEncoderCtx *>(ctx);
    const size_t out = sz3_huffman_encoder_i32_encode(&ectx->encoder, quant_inds, quant_size, buffer_pos);
    return (out == 0) ? -1 : 0;
}

int c_encoder_postprocess_encode(void *ctx) {
    auto *ectx = static_cast<CEncoderCtx *>(ctx);
    sz3_huffman_encoder_i32_postprocess_encode(&ectx->encoder);
    return 0;
}

int c_encoder_load(void *ctx, const unsigned char **buffer_pos, size_t buffer_size) {
    auto *ectx = static_cast<CEncoderCtx *>(ctx);
    size_t remaining = buffer_size;
    return sz3_huffman_encoder_i32_load(&ectx->encoder, buffer_pos, &remaining);
}

int c_encoder_decode(void *ctx, const unsigned char **buffer_pos, size_t quant_size, int **quant_inds_out) {
    auto *ectx = static_cast<CEncoderCtx *>(ctx);
    *quant_inds_out = static_cast<int *>(malloc(sizeof(int) * quant_size));
    if (*quant_inds_out == nullptr) {
        return -1;
    }
    return sz3_huffman_encoder_i32_decode(&ectx->encoder, buffer_pos, quant_size, *quant_inds_out);
}

int c_encoder_postprocess_decode(void *ctx) {
    auto *ectx = static_cast<CEncoderCtx *>(ctx);
    sz3_huffman_encoder_i32_postprocess_decode(&ectx->encoder);
    return 0;
}

size_t c_lossless_compress(void *ctx, const unsigned char *src, size_t src_size, unsigned char *dst, size_t dst_cap) {
    auto *lctx = static_cast<CLosslessCtx *>(ctx);
    return lctx->lossless.compress(src, src_size, dst, dst_cap);
}

int c_lossless_decompress(void *ctx, const unsigned char *cmp_data, size_t cmp_size, unsigned char **buffer,
                          size_t *buffer_size) {
    auto *lctx = static_cast<CLosslessCtx *>(ctx);
    unsigned char *tmp = nullptr;
    size_t out_size = 0;
    lctx->lossless.decompress(cmp_data, cmp_size, tmp, out_size);
    *buffer = tmp;
    *buffer_size = out_size;
    return 0;
}

} // namespace

int main() {
    constexpr size_t kLength = 1500;
    constexpr float kEb = 1e-3f;
    constexpr int kRadius = 32768;

    std::vector<float> input(kLength);
    for (size_t i = 0; i < kLength; i++) {
        const float x = static_cast<float>(i);
        input[i] = std::sin(0.017f * x) * 5.0f + std::cos(0.031f * x) * 2.0f + 0.003f * x;
    }

    std::vector<float> cpp_data = input;
    std::vector<float> c_data = input;

    SZ3::Config conf(kLength);
    conf.absErrorBound = kEb;
    conf.interpAlgo = SZ3::INTERP_ALGO_CUBIC;
    conf.interpDirection = 0;
    conf.interpAnchorStride = 0;
    conf.interpAlpha = 1.25;
    conf.interpBeta = 2.0;

    auto cpp_quantizer = SZ3::LinearQuantizer<float>(kEb, kRadius, true);
    auto cpp_decomposition = SZ3::make_decomposition_interpolation<float, 1>(conf, cpp_quantizer);
    auto cpp_compressor = SZ3::make_compressor_sz_generic<float, 1>(cpp_decomposition, SZ3::HuffmanEncoder<int>(),
                                                                    SZ3::Lossless_zstd());

    std::vector<unsigned char> cpp_cmp(kLength * sizeof(float) * 8, 0);
    const size_t cpp_cmp_size = cpp_compressor->compress(conf, cpp_data.data(), cpp_cmp.data(), cpp_cmp.size());
    if (cpp_cmp_size == 0) {
        return fail("C++ SZGenericCompressor compress returned 0");
    }
    std::vector<float> cpp_dec(kLength, 0.0f);
    cpp_compressor->decompress(conf, cpp_cmp.data(), cpp_cmp_size, cpp_dec.data());

    CDecompositionCtx c_decomp_ctx;
    c_decomp_ctx.conf.num_dims = 1;
    c_decomp_ctx.conf.dims[0] = kLength;
    c_decomp_ctx.conf.interp_algo = 1;
    c_decomp_ctx.conf.interp_direction = 0;
    c_decomp_ctx.conf.interp_anchor_stride = 0;
    c_decomp_ctx.conf.interp_alpha = 1.25;
    c_decomp_ctx.conf.interp_beta = 2.0;

    SZ3LineQuantizerC c_quantizer;
    sz3_line_quantizer_init(&c_quantizer, kEb, kRadius, true);
    sz3_interp_decomp_init(&c_decomp_ctx.decomp, &c_decomp_ctx.conf, &c_quantizer);

    CEncoderCtx c_encoder_ctx;
    sz3_huffman_encoder_i32_init(&c_encoder_ctx.encoder);
    CLosslessCtx c_lossless_ctx;

    SZ3_DecompositionOps_C decomp_ops{c_decomp_compress, c_decomp_get_out_range, c_decomp_size_est,
                                      c_decomp_save,     c_decomp_load,          c_decomp_decompress};
    SZ3_EncoderOps_C encoder_ops{c_encoder_preprocess_encode, c_encoder_size_est,          c_encoder_save,
                                 c_encoder_encode,             c_encoder_postprocess_encode, c_encoder_load,
                                 c_encoder_decode,             c_encoder_postprocess_decode};
    SZ3_LosslessOps_C lossless_ops{c_lossless_compress, c_lossless_decompress};

    SZ3_GenericCompressor_C c_compressor;
    sz3_generic_compressor_init(&c_compressor, &c_decomp_ctx, decomp_ops, &c_encoder_ctx, encoder_ops,
                                &c_lossless_ctx, lossless_ops);

    SZ3_Config_C c_conf{};
    c_conf.num = kLength;

    std::vector<unsigned char> c_cmp(kLength * sizeof(float) * 8, 0);
    const size_t c_cmp_size = sz3_generic_compress(&c_compressor, &c_conf, c_data.data(), c_cmp.data(), c_cmp.size());
    if (c_cmp_size == 0) {
        return fail("C SZGenericCompressor compress returned 0");
    }

    std::vector<float> c_dec(kLength, 0.0f);
    if (sz3_generic_decompress(&c_compressor, &c_conf, c_cmp.data(), c_cmp_size, c_dec.data()) != 0) {
        return fail("C SZGenericCompressor decompress failed");
    }

    // 按要求打印 C++ 与 C 实现结果的前 64 个变量，每 16 个一行。
    print_first_64_float_values("cpp_dec", cpp_dec.data(), cpp_dec.size());
    print_first_64_float_values("c_dec", c_dec.data(), c_dec.size());

    for (size_t i = 0; i < kLength; i++) {
        if (!close_enough(cpp_dec[i], c_dec[i])) {
            std::cerr << "[INFO] C/C++ first mismatch at i=" << i << ", cpp=" << cpp_dec[i] << ", c=" << c_dec[i]
                      << std::endl;
            std::cerr << "[INFO] 差异最可能来自 C 版 SZGenericCompressor 的模块适配接口："
                      << "decomposition_ops.load/encoder_ops.load 使用 buffer_size 而非剩余长度，"
                      << "以及模块需通过回调自行维护状态。" << std::endl;
            return fail("C/C++ decompressed values mismatch");
        }
        if (std::fabs(cpp_dec[i] - input[i]) > kEb * 1.2f) {
            return fail("C++ decompressed value exceeds error bound at i=" + std::to_string(i));
        }
        if (std::fabs(c_dec[i] - input[i]) > kEb * 1.2f) {
            return fail("C decompressed value exceeds error bound at i=" + std::to_string(i));
        }
    }

    sz3_huffman_encoder_i32_destroy(&c_encoder_ctx.encoder);
    sz3_interp_decomp_destroy(&c_decomp_ctx.decomp);
    sz3_line_quantizer_destroy(&c_quantizer);

    std::cout << "[PASS] SZGenericCompressor C/C++ compress+decompress test passed." << std::endl;
    return 0;
}