#ifndef SZ3_BYTEUTIL_C_H
#define SZ3_BYTEUTIL_C_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ===================== 非编码模块依赖（空函数占位） =====================
 * 说明：
 * 1) ByteUtil 的 C 版本会依赖字符串输出、动态数组管理等通用能力；
 * 2) 当前先给出空函数占位，后续可以替换为项目真实模块。
 */

/* ---- 文本缓冲区依赖（用于 floatToBinary 的字符串输出） ---- */
static inline void sz3_dep_strbuf_reset(char *buf, size_t cap) {
    (void)buf;
    (void)cap;
}

static inline void sz3_dep_strbuf_set_char(char *buf, size_t cap, size_t idx, char ch) {
    (void)buf;
    (void)cap;
    (void)idx;
    (void)ch;
}

/* ---- 可变长数组依赖（用于 vector2bytes/bytes2vector 的容器行为） ---- */
static inline void sz3_dep_dynarray_reserve_u64(void **data, size_t *cap, size_t elem_size, size_t need) {
    (void)data;
    (void)cap;
    (void)elem_size;
    (void)need;
}

/*
 * ===================== C 版联合体定义（对标 ByteUtil.hpp） =====================
 */
typedef union SZ3Lint16 {
    uint16_t usvalue;
    int16_t svalue;
    unsigned char byte[2];
} SZ3Lint16;

typedef union SZ3Lint32 {
    int32_t ivalue;
    uint32_t uivalue;
    unsigned char byte[4];
} SZ3Lint32;

typedef union SZ3Lint64 {
    int64_t lvalue;
    uint64_t ulvalue;
    unsigned char byte[8];
} SZ3Lint64;

typedef union SZ3LDouble {
    double value;
    uint64_t lvalue;
    unsigned char byte[8];
} SZ3LDouble;

typedef union SZ3LFloat {
    float value;
    uint32_t ivalue;
    unsigned char byte[4];
    uint16_t int16[2];
} SZ3LFloat;

/*
 * ===================== 基础字节转换函数 =====================
 */
static inline void sz3_sym_transform_4bytes(unsigned char data[4]) {
    unsigned char tmp = data[0];
    data[0] = data[3];
    data[3] = tmp;

    tmp = data[1];
    data[1] = data[2];
    data[2] = tmp;
}

static inline int16_t sz3_bytes_to_int16_big_endian(const unsigned char *bytes) {
    int16_t temp = 0;
    int16_t res = 0;

    temp = (int16_t)(bytes[0] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int16_t)(bytes[1] & 0xffu);
    res |= temp;

    return res;
}

static inline int32_t sz3_bytes_to_int32_big_endian(const unsigned char *bytes) {
    int32_t temp = 0;
    int32_t res = 0;

    res <<= 8;
    temp = (int32_t)(bytes[0] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int32_t)(bytes[1] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int32_t)(bytes[2] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int32_t)(bytes[3] & 0xffu);
    res |= temp;

    return res;
}

static inline int64_t sz3_bytes_to_int64_big_endian(const unsigned char *bytes) {
    int64_t temp = 0;
    int64_t res = 0;

    res <<= 8;
    temp = (int64_t)(bytes[0] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int64_t)(bytes[1] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int64_t)(bytes[2] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int64_t)(bytes[3] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int64_t)(bytes[4] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int64_t)(bytes[5] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int64_t)(bytes[6] & 0xffu);
    res |= temp;

    res <<= 8;
    temp = (int64_t)(bytes[7] & 0xffu);
    res |= temp;

    return res;
}

static inline void sz3_int16_to_bytes_big_endian(unsigned char *bytes, uint16_t num) {
    bytes[0] = (unsigned char)(num >> 8);
    bytes[1] = (unsigned char)(num);
}

static inline void sz3_int32_to_bytes_big_endian(unsigned char *bytes, uint32_t num) {
    bytes[0] = (unsigned char)(num >> 24);
    bytes[1] = (unsigned char)(num >> 16);
    bytes[2] = (unsigned char)(num >> 8);
    bytes[3] = (unsigned char)(num);
}

static inline void sz3_int64_to_bytes_big_endian(unsigned char *bytes, uint64_t num) {
    bytes[0] = (unsigned char)(num >> 56);
    bytes[1] = (unsigned char)(num >> 48);
    bytes[2] = (unsigned char)(num >> 40);
    bytes[3] = (unsigned char)(num >> 32);
    bytes[4] = (unsigned char)(num >> 24);
    bytes[5] = (unsigned char)(num >> 16);
    bytes[6] = (unsigned char)(num >> 8);
    bytes[7] = (unsigned char)(num);
}

/*
 * 将 float 转为 32 位二进制字符串。
 * - out_str 至少需要 33 字节（末尾 '\0'）。
 */
static inline void sz3_float_to_binary(float value, char out_str[33]) {
    int i;
    SZ3LFloat u;
    u.value = value;
    sz3_dep_strbuf_reset(out_str, 33);
    for (i = 0; i < 32; i++) {
        char bit = (u.ivalue % 2u) ? '1' : '0';
        sz3_dep_strbuf_set_char(out_str, 33, (size_t)(31 - i), bit);
        out_str[31 - i] = bit;
        u.ivalue >>= 1;
    }
    out_str[32] = '\0';
}

/*
 * ===================== truncateArray / truncateArrayRecover 的 C 版函数族 =====================
 * 说明：
 * - C++ 模板版本在项目中主要用于 float 场景，这里按常用类型分函数声明；
 * - binary 为“二级指针”，函数写完后会推进游标。
 */
static inline void sz3_truncate_array_f32(const float *data, size_t n, int byte_len, unsigned char **binary) {
    size_t i;
    int b;
    SZ3LFloat bytes;
    for (i = 0; i < n; i++) {
        bytes.value = data[i];
        for (b = 4 - byte_len; b < 4; b++) {
            *(*binary)++ = bytes.byte[b];
        }
    }
}

static inline void sz3_truncate_array_f64(const double *data, size_t n, int byte_len, unsigned char **binary) {
    size_t i;
    int b;
    SZ3LDouble bytes;
    for (i = 0; i < n; i++) {
        bytes.value = data[i];
        for (b = 8 - byte_len; b < 8; b++) {
            *(*binary)++ = bytes.byte[b];
        }
    }
}

static inline void sz3_truncate_array_recover_f32(const unsigned char **binary, size_t n, int byte_len, float *data) {
    size_t i;
    int b;
    SZ3LFloat bytes;
    bytes.ivalue = 0;
    for (i = 0; i < n; i++) {
        for (b = 4 - byte_len; b < 4; b++) {
            bytes.byte[b] = *(*binary)++;
        }
        data[i] = bytes.value;
    }
}

static inline void sz3_truncate_array_recover_f64(const unsigned char **binary,
                                                  size_t n,
                                                  int byte_len,
                                                  double *data) {
    size_t i;
    int b;
    SZ3LDouble bytes;
    bytes.lvalue = 0;
    for (i = 0; i < n; i++) {
        for (b = 8 - byte_len; b < 8; b++) {
            bytes.byte[b] = *(*binary)++;
        }
        data[i] = bytes.value;
    }
}

/*
 * ===================== vector_bit_width 的 C 版函数族 =====================
 */
static inline uint8_t sz3_vector_bit_width_u8(const uint8_t *data, size_t n) {
    size_t i;
    uint8_t max_value = 0;
    uint8_t bits = 0;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        if (data[i] > max_value) max_value = data[i];
    }
    while (max_value > 0) {
        max_value >>= 1;
        ++bits;
    }
    return bits;
}

static inline uint8_t sz3_vector_bit_width_u16(const uint16_t *data, size_t n) {
    size_t i;
    uint16_t max_value = 0;
    uint8_t bits = 0;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        if (data[i] > max_value) max_value = data[i];
    }
    while (max_value > 0) {
        max_value >>= 1;
        ++bits;
    }
    return bits;
}

static inline uint8_t sz3_vector_bit_width_u32(const uint32_t *data, size_t n) {
    size_t i;
    uint32_t max_value = 0;
    uint8_t bits = 0;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        if (data[i] > max_value) max_value = data[i];
    }
    while (max_value > 0) {
        max_value >>= 1;
        ++bits;
    }
    return bits;
}

static inline uint8_t sz3_vector_bit_width_u64(const uint64_t *data, size_t n) {
    size_t i;
    uint64_t max_value = 0;
    uint8_t bits = 0;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        if (data[i] > max_value) max_value = data[i];
    }
    while (max_value > 0) {
        max_value >>= 1;
        ++bits;
    }
    return bits;
}

/*
 * ===================== vector2bytes 的 C 版函数族 =====================
 * 说明：
 * - out_bytes 为二级指针，写入后自动推进。
 */
static inline void sz3_vector2bytes_u8(const uint8_t *data, size_t n, uint8_t bit_width, unsigned char **out_bytes) {
    size_t i;
    size_t current_bit = 0;
    size_t byte_index = 0;
    unsigned char current_byte = 0;
    if (n == 0) return;
    for (i = 0; i < n; i++) {
        uint8_t value = data[i];
        size_t bits_remaining = bit_width;
        while (bits_remaining > 0) {
            size_t offset = current_bit % 8;
            size_t space_in_current_byte = 8 - offset;
            size_t bits_to_write = bits_remaining < space_in_current_byte ? bits_remaining : space_in_current_byte;
            size_t bits_shift = ((size_t)bit_width - bits_remaining);
            unsigned char mask = (unsigned char)((1u << bits_to_write) - 1u);
            unsigned char bits_to_store = (unsigned char)((value >> bits_shift) & mask);
            current_byte |= (unsigned char)(bits_to_store << offset);
            current_bit += bits_to_write;
            bits_remaining -= bits_to_write;
            if ((current_bit % 8) == 0) {
                (*out_bytes)[byte_index++] = current_byte;
                current_byte = 0;
            }
        }
    }
    if ((current_bit % 8) != 0) {
        (*out_bytes)[byte_index++] = current_byte;
    }
    *out_bytes += byte_index;
}

static inline void sz3_vector2bytes_u16(const uint16_t *data, size_t n, uint8_t bit_width, unsigned char **out_bytes) {
    size_t i;
    size_t current_bit = 0;
    size_t byte_index = 0;
    unsigned char current_byte = 0;
    if (n == 0) return;
    for (i = 0; i < n; i++) {
        uint16_t value = data[i];
        size_t bits_remaining = bit_width;
        while (bits_remaining > 0) {
            size_t offset = current_bit % 8;
            size_t space_in_current_byte = 8 - offset;
            size_t bits_to_write = bits_remaining < space_in_current_byte ? bits_remaining : space_in_current_byte;
            size_t bits_shift = ((size_t)bit_width - bits_remaining);
            uint16_t mask = (uint16_t)((1u << bits_to_write) - 1u);
            unsigned char bits_to_store = (unsigned char)((value >> bits_shift) & mask);
            current_byte |= (unsigned char)(bits_to_store << offset);
            current_bit += bits_to_write;
            bits_remaining -= bits_to_write;
            if ((current_bit % 8) == 0) {
                (*out_bytes)[byte_index++] = current_byte;
                current_byte = 0;
            }
        }
    }
    if ((current_bit % 8) != 0) {
        (*out_bytes)[byte_index++] = current_byte;
    }
    *out_bytes += byte_index;
}

static inline void sz3_vector2bytes_u32(const uint32_t *data, size_t n, uint8_t bit_width, unsigned char **out_bytes) {
    size_t i;
    size_t current_bit = 0;
    size_t byte_index = 0;
    unsigned char current_byte = 0;
    if (n == 0) return;
    for (i = 0; i < n; i++) {
        uint32_t value = data[i];
        size_t bits_remaining = bit_width;
        while (bits_remaining > 0) {
            size_t offset = current_bit % 8;
            size_t space_in_current_byte = 8 - offset;
            size_t bits_to_write = bits_remaining < space_in_current_byte ? bits_remaining : space_in_current_byte;
            size_t bits_shift = ((size_t)bit_width - bits_remaining);
            uint32_t mask = (uint32_t)((1ull << bits_to_write) - 1ull);
            unsigned char bits_to_store = (unsigned char)((value >> bits_shift) & mask);
            current_byte |= (unsigned char)(bits_to_store << offset);
            current_bit += bits_to_write;
            bits_remaining -= bits_to_write;
            if ((current_bit % 8) == 0) {
                (*out_bytes)[byte_index++] = current_byte;
                current_byte = 0;
            }
        }
    }
    if ((current_bit % 8) != 0) {
        (*out_bytes)[byte_index++] = current_byte;
    }
    *out_bytes += byte_index;
}

static inline void sz3_vector2bytes_u64(const uint64_t *data, size_t n, uint8_t bit_width, unsigned char **out_bytes) {
    size_t i;
    size_t current_bit = 0;
    size_t byte_index = 0;
    unsigned char current_byte = 0;
    if (n == 0) return;
    for (i = 0; i < n; i++) {
        uint64_t value = data[i];
        size_t bits_remaining = bit_width;
        while (bits_remaining > 0) {
            size_t offset = current_bit % 8;
            size_t space_in_current_byte = 8 - offset;
            size_t bits_to_write = bits_remaining < space_in_current_byte ? bits_remaining : space_in_current_byte;
            size_t bits_shift = ((size_t)bit_width - bits_remaining);
            uint64_t mask = (uint64_t)((1ull << bits_to_write) - 1ull);
            unsigned char bits_to_store = (unsigned char)((value >> bits_shift) & mask);
            current_byte |= (unsigned char)(bits_to_store << offset);
            current_bit += bits_to_write;
            bits_remaining -= bits_to_write;
            if ((current_bit % 8) == 0) {
                (*out_bytes)[byte_index++] = current_byte;
                current_byte = 0;
            }
        }
    }
    if ((current_bit % 8) != 0) {
        (*out_bytes)[byte_index++] = current_byte;
    }
    *out_bytes += byte_index;
}

/*
 * ===================== bytes2vector 的 C 版函数族 =====================
 * 说明：
 * - in_bytes 为二级指针，读取后自动推进；
 * - out_data 由调用方分配，长度至少为 num_elements。
 */
static inline void sz3_bytes2vector_u8(const unsigned char **in_bytes,
                                       uint8_t bit_width,
                                       size_t num_elements,
                                       uint8_t *out_data) {
    size_t i, j;
    size_t total_bits = num_elements * bit_width;
    size_t total_bytes = (total_bits + 7) / 8;
    for (i = 0; i < num_elements; ++i) {
        uint8_t value = 0;
        for (j = 0; j < bit_width; ++j) {
            size_t bit_index = i * bit_width + j;
            size_t byte_index = bit_index / 8;
            size_t bit_offset = bit_index % 8;
            value |= (uint8_t)((((*in_bytes)[byte_index] >> bit_offset) & 1u) << j);
        }
        out_data[i] = value;
    }
    *in_bytes += total_bytes;
}

static inline void sz3_bytes2vector_u16(const unsigned char **in_bytes,
                                        uint8_t bit_width,
                                        size_t num_elements,
                                        uint16_t *out_data) {
    size_t i, j;
    size_t total_bits = num_elements * bit_width;
    size_t total_bytes = (total_bits + 7) / 8;
    for (i = 0; i < num_elements; ++i) {
        uint16_t value = 0;
        for (j = 0; j < bit_width; ++j) {
            size_t bit_index = i * bit_width + j;
            size_t byte_index = bit_index / 8;
            size_t bit_offset = bit_index % 8;
            value |= (uint16_t)((((*in_bytes)[byte_index] >> bit_offset) & 1u) << j);
        }
        out_data[i] = value;
    }
    *in_bytes += total_bytes;
}

static inline void sz3_bytes2vector_u32(const unsigned char **in_bytes,
                                        uint8_t bit_width,
                                        size_t num_elements,
                                        uint32_t *out_data) {
    size_t i, j;
    size_t total_bits = num_elements * bit_width;
    size_t total_bytes = (total_bits + 7) / 8;
    for (i = 0; i < num_elements; ++i) {
        uint32_t value = 0;
        for (j = 0; j < bit_width; ++j) {
            size_t bit_index = i * bit_width + j;
            size_t byte_index = bit_index / 8;
            size_t bit_offset = bit_index % 8;
            value |= (uint32_t)((((*in_bytes)[byte_index] >> bit_offset) & 1u) << j);
        }
        out_data[i] = value;
    }
    *in_bytes += total_bytes;
}

static inline void sz3_bytes2vector_u64(const unsigned char **in_bytes,
                                        uint8_t bit_width,
                                        size_t num_elements,
                                        uint64_t *out_data) {
    size_t i, j;
    size_t total_bits = num_elements * bit_width;
    size_t total_bytes = (total_bits + 7) / 8;
    for (i = 0; i < num_elements; ++i) {
        uint64_t value = 0;
        for (j = 0; j < bit_width; ++j) {
            size_t bit_index = i * bit_width + j;
            size_t byte_index = bit_index / 8;
            size_t bit_offset = bit_index % 8;
            value |= (uint64_t)((((*in_bytes)[byte_index] >> bit_offset) & 1u) << j);
        }
        out_data[i] = value;
    }
    *in_bytes += total_bytes;
}

#ifdef __cplusplus
}
#endif

#endif /* SZ3_BYTEUTIL_C_H */
