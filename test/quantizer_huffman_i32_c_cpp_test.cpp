#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "SZ3/encoder/HuffmanEncoder.hpp"
#include "SZ3/encoder/HuffmanEncoder_i32_c.h"
#include "SZ3/quantizer/LinearQuantizer.hpp"
#include "SZ3/quantizer/line_quantizer_c.h"

namespace {

bool almost_equal(float a, float b, float eps = 1e-6f) {
    return std::fabs(a - b) <= eps;
}

int fail(const std::string &msg) {
    std::cerr << "[FAIL] " << msg << std::endl;
    return 1;
}

void print_first_64_int_values(const char *name, const std::vector<int> &values) {
    const size_t count = std::min<size_t>(64, values.size());
    std::printf("%s (first %zu values):\n", name, count);
    for (size_t i = 0; i < count; i++) {
        std::printf("%d", values[i]);
        if ((i + 1) % 16 == 0 || i + 1 == count) {
            std::printf("\n");
        } else {
            std::printf(", ");
        }
    }
}

void print_first_64_u8_values(const char *name, const unsigned char *data, size_t size) {
    const size_t count = std::min<size_t>(64, size);
    std::printf("%s (first %zu values):\n", name, count);
    for (size_t i = 0; i < count; i++) {
        std::printf("%u", static_cast<unsigned int>(data[i]));
        if ((i + 1) % 16 == 0 || i + 1 == count) {
            std::printf("\n");
        } else {
            std::printf(", ");
        }
    }
}

} // namespace

int main() {
    std::vector<float> input;
    input.reserve(4096);
    for (size_t i = 0; i < 4096; i++) {
        const float x = static_cast<float>(i) * 0.13f;
        input.push_back(std::sin(x) * 80.0f + std::cos(x * 0.7f) * 20.0f + static_cast<float>(i % 17 - 8));
    }

    constexpr float eb = 0.01f;
    constexpr int radius = 32768;

    std::vector<float> cpp_data = input;
    std::vector<float> c_data = input;
    std::vector<int> cpp_quant_index;
    std::vector<int> c_quant_index;
    cpp_quant_index.reserve(input.size());
    c_quant_index.reserve(input.size());

    SZ3::LinearQuantizer<float> cpp_quantizer(eb, radius, true);
    SZ3LineQuantizerC c_quantizer;
    sz3_line_quantizer_init(&c_quantizer, eb, radius, true);

    for (size_t i = 0; i < input.size(); i++) {
        float cpp_val = cpp_data[i];
        const int cpp_q = cpp_quantizer.quantize_and_overwrite(cpp_val, 0.0f);
        cpp_data[i] = cpp_val;
        cpp_quant_index.push_back(cpp_q);

        float c_val = c_data[i];
        const int c_q = sz3_line_quantizer_quantize_and_overwrite(&c_quantizer, &c_val, 0.0f);
        c_data[i] = c_val;
        c_quant_index.push_back(c_q);

        if (cpp_q != c_q) {
            return fail("LinearQuantizer quant index mismatch at i=" + std::to_string(i));
        }
        if (!almost_equal(cpp_data[i], c_data[i])) {
            return fail("LinearQuantizer overwritten value mismatch at i=" + std::to_string(i));
        }
    }

    const int min_q = *std::min_element(cpp_quant_index.begin(), cpp_quant_index.end());
    const int max_q = *std::max_element(cpp_quant_index.begin(), cpp_quant_index.end());
    const int state_num = max_q - min_q + 2;

    print_first_64_int_values("cpp_quant_index", cpp_quant_index);
    print_first_64_int_values("c_quant_index", c_quant_index);

    SZ3::HuffmanEncoder<int> cpp_huffman;
    cpp_huffman.preprocess_encode(cpp_quant_index.data(), cpp_quant_index.size(), state_num);

    std::vector<unsigned char> cpp_encoded(sizeof(size_t) + cpp_huffman.size_est() + 64, 0);
    unsigned char *cpp_write_ptr = cpp_encoded.data();
    const size_t cpp_out_size = cpp_huffman.encode(cpp_quant_index.data(), cpp_quant_index.size(), cpp_write_ptr);
    if (cpp_out_size == 0) {
        return fail("C++ Huffman encode returned 0");
    }

    SZ3HuffmanEncoderI32 c_huffman;
    sz3_huffman_encoder_i32_init(&c_huffman);
    if (sz3_huffman_encoder_i32_preprocess_encode(&c_huffman, c_quant_index.data(), c_quant_index.size(), state_num) != 0) {
        return fail("C Huffman(i32) preprocess_encode failed");
    }

    std::vector<unsigned char> c_encoded(sizeof(size_t) + sz3_huffman_encoder_i32_size_est(&c_huffman) + 64, 0);
    unsigned char *c_write_ptr = c_encoded.data();
    const size_t c_out_size = sz3_huffman_encoder_i32_encode(&c_huffman, c_quant_index.data(), c_quant_index.size(), &c_write_ptr);
    if (c_out_size == 0) {
        return fail("C Huffman(i32) encode returned 0");
    }

    if (cpp_out_size != c_out_size) {
        return fail("Huffman encoded size mismatch, cpp=" + std::to_string(cpp_out_size) + ", c=" + std::to_string(c_out_size));
    }

    print_first_64_u8_values("cpp_encoded", cpp_encoded.data(), cpp_encoded.size());
    print_first_64_u8_values("c_encoded", c_encoded.data(), c_encoded.size());

    const size_t payload_size = sizeof(size_t) + cpp_out_size;
    if (std::memcmp(cpp_encoded.data(), c_encoded.data(), payload_size) != 0) {
        return fail("Huffman encoded payload mismatch between C++ and C(i32) implementations");
    }

    const unsigned char *cpp_read_ptr = cpp_encoded.data();
    const std::vector<int> cpp_decoded = cpp_huffman.decode(cpp_read_ptr, cpp_quant_index.size());
    for (size_t i = 0; i < cpp_decoded.size(); i++) {
        if (cpp_decoded[i] != cpp_quant_index[i]) {
            return fail("C++ Huffman decode mismatch at i=" + std::to_string(i));
        }
    }

    cpp_huffman.postprocess_encode();
    cpp_huffman.postprocess_decode();
    sz3_huffman_encoder_i32_postprocess_encode(&c_huffman);
    sz3_huffman_encoder_i32_destroy(&c_huffman);
    sz3_line_quantizer_destroy(&c_quantizer);

    std::cout << "[PASS] Quantizer + HuffmanEncoder_i32 C/C++ consistency checks passed." << std::endl;
    return 0;
}
