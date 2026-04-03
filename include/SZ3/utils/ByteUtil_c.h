#ifndef SZ3_BYTEUTIL_C_H
#define SZ3_BYTEUTIL_C_H

#include <stddef.h>
#include <stdint.h>

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
void sz3_sym_transform_4bytes(unsigned char data[4]);

int16_t sz3_bytes_to_int16_big_endian(const unsigned char *bytes);
int32_t sz3_bytes_to_int32_big_endian(const unsigned char *bytes);
int64_t sz3_bytes_to_int64_big_endian(const unsigned char *bytes);

void sz3_int16_to_bytes_big_endian(unsigned char *bytes, uint16_t num);
void sz3_int32_to_bytes_big_endian(unsigned char *bytes, uint32_t num);
void sz3_int64_to_bytes_big_endian(unsigned char *bytes, uint64_t num);

/*
 * 将 float 转为 32 位二进制字符串。
 * - out_str 至少需要 33 字节（末尾 '\0'）。
 */
void sz3_float_to_binary(float value, char out_str[33]);

/*
 * ===================== truncateArray / truncateArrayRecover 的 C 版函数族 =====================
 * 说明：
 * - C++ 模板版本在项目中主要用于 float 场景，这里按常用类型分函数声明；
 * - binary 为“二级指针”，函数写完后会推进游标。
 */
void sz3_truncate_array_f32(const float *data, size_t n, int byte_len, unsigned char **binary);
void sz3_truncate_array_f64(const double *data, size_t n, int byte_len, unsigned char **binary);

void sz3_truncate_array_recover_f32(const unsigned char **binary, size_t n, int byte_len, float *data);
void sz3_truncate_array_recover_f64(const unsigned char **binary, size_t n, int byte_len, double *data);

/*
 * ===================== vector_bit_width 的 C 版函数族 =====================
 */
uint8_t sz3_vector_bit_width_u8(const uint8_t *data, size_t n);
uint8_t sz3_vector_bit_width_u16(const uint16_t *data, size_t n);
uint8_t sz3_vector_bit_width_u32(const uint32_t *data, size_t n);
uint8_t sz3_vector_bit_width_u64(const uint64_t *data, size_t n);

/*
 * ===================== vector2bytes 的 C 版函数族 =====================
 * 说明：
 * - out_bytes 为二级指针，写入后自动推进。
 */
void sz3_vector2bytes_u8(const uint8_t *data, size_t n, uint8_t bit_width, unsigned char **out_bytes);
void sz3_vector2bytes_u16(const uint16_t *data, size_t n, uint8_t bit_width, unsigned char **out_bytes);
void sz3_vector2bytes_u32(const uint32_t *data, size_t n, uint8_t bit_width, unsigned char **out_bytes);
void sz3_vector2bytes_u64(const uint64_t *data, size_t n, uint8_t bit_width, unsigned char **out_bytes);

/*
 * ===================== bytes2vector 的 C 版函数族 =====================
 * 说明：
 * - in_bytes 为二级指针，读取后自动推进；
 * - out_data 由调用方分配，长度至少为 num_elements。
 */
void sz3_bytes2vector_u8(const unsigned char **in_bytes,
                         uint8_t bit_width,
                         size_t num_elements,
                         uint8_t *out_data);

void sz3_bytes2vector_u16(const unsigned char **in_bytes,
                          uint8_t bit_width,
                          size_t num_elements,
                          uint16_t *out_data);

void sz3_bytes2vector_u32(const unsigned char **in_bytes,
                          uint8_t bit_width,
                          size_t num_elements,
                          uint32_t *out_data);

void sz3_bytes2vector_u64(const unsigned char **in_bytes,
                          uint8_t bit_width,
                          size_t num_elements,
                          uint64_t *out_data);

#ifdef __cplusplus
}
#endif

#endif /* SZ3_BYTEUTIL_C_H */
