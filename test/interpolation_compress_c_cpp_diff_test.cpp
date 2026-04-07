#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "SZ3/utils/Config.hpp"
#include "SZ3/utils/BlockwiseIterator.hpp"
#include "SZ3/decomposition/InterpolationDecomposition.hpp"
#include "SZ3/decomposition/InterpolationDecomposition_c.h"
#include "SZ3/quantizer/LinearQuantizer.hpp"
#include "SZ3/quantizer/line_quantizer_c.h"

namespace {

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

    SZ3InterpConfigC c_conf{};
    c_conf.num_dims = 1;
    c_conf.dims[0] = kLength;
    c_conf.interp_algo = 1;
    c_conf.interp_direction = 0;
    c_conf.interp_anchor_stride = 0;
    c_conf.interp_alpha = 1.25;
    c_conf.interp_beta = 2.0;

    SZ3LineQuantizerC c_quantizer;
    sz3_line_quantizer_init(&c_quantizer, kEb, kRadius, true);

    SZ3InterpolationDecompositionC c_decomp;
    sz3_interp_decomp_init(&c_decomp, &c_conf, &c_quantizer);

    size_t c_quant_count = 0;
    int *c_quant_inds = sz3_interp_decomp_compress(&c_decomp, &c_conf, c_data.data(), &c_quant_count);
    if (c_quant_inds == nullptr) {
        sz3_interp_decomp_destroy(&c_decomp);
        return fail("sz3_interp_decomp_compress returned nullptr");
    }

    print_first_64_int_values("cpp_quant_inds", cpp_quant_inds.data(), cpp_quant_inds.size());
    print_first_64_int_values("c_quant_inds", c_quant_inds, c_quant_count);

    if (cpp_quant_inds.size() != c_quant_count) {
        sz3_interp_decomp_destroy(&c_decomp);
        return fail("compress output size mismatch, cpp=" + std::to_string(cpp_quant_inds.size()) +
                    ", c=" + std::to_string(c_quant_count));
    }

    size_t mismatch_count = 0;
    size_t first_mismatch_idx = 0;
    for (size_t i = 0; i < cpp_quant_inds.size(); i++) {
        if (cpp_quant_inds[i] != c_quant_inds[i]) {
            if (mismatch_count == 0) {
                first_mismatch_idx = i;
            }
            mismatch_count++;
        }
    }

    sz3_interp_decomp_destroy(&c_decomp);

    if (mismatch_count > 0) {
        std::cerr << "[INFO] first mismatch at i=" << first_mismatch_idx << ", cpp=" << cpp_quant_inds[first_mismatch_idx]
                  << ", c=" << c_quant_inds[first_mismatch_idx] << std::endl;
        std::cerr << "[INFO] total mismatches=" << mismatch_count << std::endl;
        return fail("InterpolationDecomposition compress outputs differ between C and C++ implementations");
    }

    std::cout << "[PASS] InterpolationDecomposition compress C/C++ consistency checks passed." << std::endl;
    return 0;
}
