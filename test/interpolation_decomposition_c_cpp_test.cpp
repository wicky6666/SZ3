#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "SZ3/utils/Config.hpp"
#include "SZ3/utils/BlockwiseIterator.hpp"
#include "SZ3/decomposition/InterpolationDecomposition.hpp"
#include "SZ3/decomposition/InterpolationDecomposition_c.h"
#include "SZ3/quantizer/LinearQuantizer.hpp"
#include "SZ3/quantizer/line_quantizer_c.h"

namespace {

bool almost_equal(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

int fail(const std::string &msg) {
    std::cerr << "[FAIL] " << msg << std::endl;
    return 1;
}

void print_first_64_int_values(const char *name, const int *values, size_t size) {
    const size_t count = std::min<size_t>(64, size);
    std::cout << name << " (first " << count << " values):" << std::endl;
    for (size_t i = 0; i < count; i++) {
        std::cout << values[i];
        if ((i + 1) % 16 == 0 || i + 1 == count) {
            std::cout << std::endl;
        } else {
            std::cout << ", ";
        }
    }
}

void print_first_64_float_values(const char *name, const float *values, size_t size) {
    const size_t count = std::min<size_t>(64, size);
    std::cout << name << " (first " << count << " values):" << std::endl;
    for (size_t i = 0; i < count; i++) {
        std::cout << values[i];
        if ((i + 1) % 16 == 0 || i + 1 == count) {
            std::cout << std::endl;
        } else {
            std::cout << ", ";
        }
    }
}

}  // namespace

int main() {
    constexpr size_t kLength = 1000;
    constexpr float kEb = 1e-3f;
    constexpr int kRadius = 32768;

    std::vector<float> input(kLength);
    for (size_t i = 0; i < kLength; i++) {
        input[i] = std::sin(static_cast<float>(i) * 0.021f) * 20.0f + std::cos(static_cast<float>(i) * 0.037f) * 3.0f +
                   static_cast<float>(i) * 0.001f;
    }

    std::vector<float> cpp_data = input;
    std::vector<float> c_data = input;

    SZ3::Config cpp_conf(kLength);
    cpp_conf.absErrorBound = kEb;
    cpp_conf.interpAlgo = SZ3::INTERP_ALGO_CUBIC;
    cpp_conf.interpDirection = 0;
    cpp_conf.interpAnchorStride = 0;
    cpp_conf.interpAlpha = 1.25;
    cpp_conf.interpBeta = 2.0;

    SZ3::LinearQuantizer<float> cpp_quantizer(kEb, kRadius, true);
    auto cpp_decomp = SZ3::make_decomposition_interpolation<float, 1>(cpp_conf, cpp_quantizer);
    std::vector<int> cpp_quant_inds = cpp_decomp.compress(cpp_conf, cpp_data.data());
    std::vector<float> cpp_dec(kLength, 0.0f);
    cpp_decomp.decompress(cpp_conf, cpp_quant_inds, cpp_dec.data());

    SZ3InterpConfigC c_conf{};
    c_conf.num_dims = 1;
    c_conf.dims[0] = kLength;
    c_conf.interp_algo = 1;
    c_conf.interp_direction = 0;
    c_conf.interp_anchor_stride = 0;
    c_conf.interp_alpha = 1.25;
    c_conf.interp_beta = 2.0;

    SZ3LineQuantizerC c_quantizer;
    sz3_line_quantizer_init(&c_quantizer, kEb, kRadius, 1);

    SZ3InterpolationDecompositionC c_decomp;
    sz3_interp_decomp_init(&c_decomp, &c_conf, &c_quantizer);

    size_t c_quant_count = 0;
    int *c_quant_inds = sz3_interp_decomp_compress(&c_decomp, &c_conf, c_data.data(), &c_quant_count);
    if (c_quant_inds == nullptr) {
        return fail("C interpolation compress returned nullptr");
    }

    std::vector<float> c_dec(kLength, std::numeric_limits<float>::quiet_NaN());
    sz3_interp_decomp_decompress(&c_decomp, &c_conf, c_quant_inds, c_dec.data(), c_quant_count);

    print_first_64_int_values("cpp_quant_inds", cpp_quant_inds.data(), cpp_quant_inds.size());
    print_first_64_int_values("c_quant_inds", c_quant_inds, c_quant_count);
    print_first_64_float_values("cpp_dec", cpp_dec.data(), cpp_dec.size());
    print_first_64_float_values("c_dec", c_dec.data(), c_dec.size());

    bool quant_match = true;
    for (size_t i = 0; i < cpp_quant_inds.size(); i++) {
        if (cpp_quant_inds[i] != c_quant_inds[i]) {
            quant_match = false;
            std::cerr << "[INFO] quant index mismatch at i=" << i << ", cpp=" << cpp_quant_inds[i]
                      << ", c=" << c_quant_inds[i] << std::endl;
            break;
        }
    }

    bool dec_match = true;
    size_t dec_mismatch_idx = 0;
    for (size_t i = 0; i < kLength; i++) {
        if (!almost_equal(cpp_dec[i], c_dec[i])) {
            dec_match = false;
            dec_mismatch_idx = i;
            break;
        }
    }

    sz3_interp_decomp_destroy(&c_decomp);

    if (!quant_match) {
        return fail("InterpolationDecomposition C/C++ quant indices mismatch");
    }

    if (!dec_match) {
        std::cerr << "[INFO] decompressed mismatch at i=" << dec_mismatch_idx << ", cpp=" << cpp_dec[dec_mismatch_idx]
                  << ", c=" << c_dec[dec_mismatch_idx] << std::endl;
        return fail("InterpolationDecomposition C decompress differs from C++ implementation");
    }

    std::cout << "[PASS] InterpolationDecomposition C/C++ consistency checks passed." << std::endl;
    return 0;
}
