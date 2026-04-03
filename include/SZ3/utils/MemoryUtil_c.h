#ifndef SZ3_MEMORYUTIL_C_H
#define SZ3_MEMORYUTIL_C_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ===================== 非编码模块依赖（空函数占位） =====================
 * 说明：
 * 1) MemoryUtil 的核心逻辑只依赖“边界检查/错误上报”等通用能力；
 * 2) 这里先按要求给出空函数占位，后续可替换为统一日志与错误处理模块。
 */
static inline void sz3_dep_on_memory_error(const char *msg) {
    /* 占位错误处理：当前用 printf 输出，后续可替换为统一日志系统 */
    printf("[SZ3][MemoryUtil_c] %s\n", msg != NULL ? msg : "unknown memory error");
}

/*
 * ===================== 端序检测（对标 MemoryUtil.hpp） =====================
 * 约定：与 C++ 版本一致，字节流统一按 little-endian 存储。
 */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SZ3_C_BIG_ENDIAN 1
#elif defined(__BIG_ENDIAN__) || defined(__ARMEB__) || defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || defined(_MIPSEB) || defined(__MIPSEB)
#define SZ3_C_BIG_ENDIAN 1
#else
#define SZ3_C_BIG_ENDIAN 0
#endif

/*
 * ===================== 字节交换辅助函数 =====================
 */
static inline uint16_t sz3_bswap16(uint16_t x) {
#if defined(_MSC_VER)
    return _byteswap_ushort(x);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16(x);
#else
    return (uint16_t)((x >> 8) | (x << 8));
#endif
}

static inline uint32_t sz3_bswap32(uint32_t x) {
#if defined(_MSC_VER)
    return _byteswap_ulong(x);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32(x);
#else
    return ((x & 0x000000FFu) << 24) | ((x & 0x0000FF00u) << 8) | ((x & 0x00FF0000u) >> 8) |
           ((x & 0xFF000000u) >> 24);
#endif
}

static inline uint64_t sz3_bswap64(uint64_t x) {
#if defined(_MSC_VER)
    return _byteswap_uint64(x);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
#else
    return ((x & 0x00000000000000FFull) << 56) | ((x & 0x000000000000FF00ull) << 40) |
           ((x & 0x0000000000FF0000ull) << 24) | ((x & 0x00000000FF000000ull) << 8) |
           ((x & 0x000000FF00000000ull) >> 8) | ((x & 0x0000FF0000000000ull) >> 24) |
           ((x & 0x00FF000000000000ull) >> 40) | ((x & 0xFF00000000000000ull) >> 56);
#endif
}

/*
 * 通用 byteswap（按元素大小处理）
 * - elem_size = 1/2/4/8 时走快速路径；
 * - 其它大小走通用字节反转。
 */
static inline void sz3_byteswap_inplace(void *value, size_t elem_size) {
    size_t i;
    unsigned char *p = (unsigned char *)value;

    if (elem_size == 1) {
        return;
    }
    if (elem_size == 2) {
        uint16_t t;
        memcpy(&t, value, sizeof(t));
        t = sz3_bswap16(t);
        memcpy(value, &t, sizeof(t));
        return;
    }
    if (elem_size == 4) {
        uint32_t t;
        memcpy(&t, value, sizeof(t));
        t = sz3_bswap32(t);
        memcpy(value, &t, sizeof(t));
        return;
    }
    if (elem_size == 8) {
        uint64_t t;
        memcpy(&t, value, sizeof(t));
        t = sz3_bswap64(t);
        memcpy(value, &t, sizeof(t));
        return;
    }

    for (i = 0; i < elem_size / 2; i++) {
        unsigned char tmp = p[i];
        p[i] = p[elem_size - 1 - i];
        p[elem_size - 1 - i] = tmp;
    }
}

/*
 * ===================== 与 C++ 模板函数一一对应的 C 接口 =====================
 * 对应关系：
 * 1) read(array, num_elements, pos, remaining)
 * 2) read(array, num_elements, pos)
 * 3) read(var, pos)
 * 4) read(var, pos, remaining)
 * 5) write(array, num_elements, pos)
 * 6) write(var, pos)
 */

/*
 * 读取数组（带 remaining_length 检查）
 */
static inline void sz3_read_array_with_remaining(void *array,
                                                 size_t num_elements,
                                                 size_t elem_size,
                                                 const unsigned char **compressed_data_pos,
                                                 size_t *remaining_length) {
#if SZ3_C_BIG_ENDIAN
    size_t i;
#endif
    size_t total_bytes;

    if (array == NULL || compressed_data_pos == NULL || *compressed_data_pos == NULL || remaining_length == NULL) {
        sz3_dep_on_memory_error("sz3_read_array_with_remaining: invalid input pointer");
        return;
    }

    total_bytes = num_elements * elem_size;
    assert(total_bytes <= *remaining_length);

    memcpy(array, *compressed_data_pos, total_bytes);

#if SZ3_C_BIG_ENDIAN
    for (i = 0; i < num_elements; i++) {
        sz3_byteswap_inplace((unsigned char *)array + i * elem_size, elem_size);
    }
#endif

    *remaining_length -= total_bytes;
    *compressed_data_pos += total_bytes;
}

/*
 * 读取数组（不带 remaining_length）
 */
static inline void sz3_read_array(void *array,
                                  size_t num_elements,
                                  size_t elem_size,
                                  const unsigned char **compressed_data_pos) {
#if SZ3_C_BIG_ENDIAN
    size_t i;
#endif
    size_t total_bytes;

    if (array == NULL || compressed_data_pos == NULL || *compressed_data_pos == NULL) {
        sz3_dep_on_memory_error("sz3_read_array: invalid input pointer");
        return;
    }

    total_bytes = num_elements * elem_size;
    memcpy(array, *compressed_data_pos, total_bytes);

#if SZ3_C_BIG_ENDIAN
    for (i = 0; i < num_elements; i++) {
        sz3_byteswap_inplace((unsigned char *)array + i * elem_size, elem_size);
    }
#endif

    *compressed_data_pos += total_bytes;
}

/*
 * 读取单个变量（不带 remaining_length）
 */
static inline void sz3_read_var(void *var, size_t elem_size, const unsigned char **compressed_data_pos) {
    if (var == NULL || compressed_data_pos == NULL || *compressed_data_pos == NULL) {
        sz3_dep_on_memory_error("sz3_read_var: invalid input pointer");
        return;
    }

    memcpy(var, *compressed_data_pos, elem_size);

#if SZ3_C_BIG_ENDIAN
    sz3_byteswap_inplace(var, elem_size);
#endif

    *compressed_data_pos += elem_size;
}

/*
 * 读取单个变量（带 remaining_length 检查）
 */
static inline void sz3_read_var_with_remaining(void *var,
                                               size_t elem_size,
                                               const unsigned char **compressed_data_pos,
                                               size_t *remaining_length) {
    if (var == NULL || compressed_data_pos == NULL || *compressed_data_pos == NULL || remaining_length == NULL) {
        sz3_dep_on_memory_error("sz3_read_var_with_remaining: invalid input pointer");
        return;
    }

    assert(elem_size <= *remaining_length);
    memcpy(var, *compressed_data_pos, elem_size);

#if SZ3_C_BIG_ENDIAN
    sz3_byteswap_inplace(var, elem_size);
#endif

    *remaining_length -= elem_size;
    *compressed_data_pos += elem_size;
}

/*
 * 写入数组
 */
static inline void sz3_write_array(const void *array,
                                   size_t num_elements,
                                   size_t elem_size,
                                   unsigned char **compressed_data_pos) {
#if SZ3_C_BIG_ENDIAN
    size_t i;
#endif
    size_t total_bytes;

    if (array == NULL || compressed_data_pos == NULL || *compressed_data_pos == NULL) {
        sz3_dep_on_memory_error("sz3_write_array: invalid input pointer");
        return;
    }

    total_bytes = num_elements * elem_size;
    memcpy(*compressed_data_pos, array, total_bytes);

#if SZ3_C_BIG_ENDIAN
    for (i = 0; i < num_elements; i++) {
        sz3_byteswap_inplace((*compressed_data_pos) + i * elem_size, elem_size);
    }
#endif

    *compressed_data_pos += total_bytes;
}

/*
 * 写入单个变量
 */
static inline void sz3_write_var(const void *var, size_t elem_size, unsigned char **compressed_data_pos) {
    if (var == NULL || compressed_data_pos == NULL || *compressed_data_pos == NULL) {
        sz3_dep_on_memory_error("sz3_write_var: invalid input pointer");
        return;
    }

#if SZ3_C_BIG_ENDIAN
    {
        unsigned char temp_stack[16];
        unsigned char *temp = temp_stack;

        if (elem_size > sizeof(temp_stack)) {
            temp = (unsigned char *)malloc(elem_size);
            if (temp == NULL) {
                sz3_dep_on_memory_error("sz3_write_var: malloc failed");
                return;
            }
        }

        memcpy(temp, var, elem_size);
        sz3_byteswap_inplace(temp, elem_size);
        memcpy(*compressed_data_pos, temp, elem_size);
        *compressed_data_pos += elem_size;

        if (temp != temp_stack) {
            free(temp);
        }
    }
#else
    memcpy(*compressed_data_pos, var, elem_size);
    *compressed_data_pos += elem_size;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* SZ3_MEMORYUTIL_C_H */
