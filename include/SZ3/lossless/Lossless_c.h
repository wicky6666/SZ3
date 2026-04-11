#ifndef SZ3_LOSSLESS_C_H
#define SZ3_LOSSLESS_C_H

#include <stddef.h>

#include "SZ3/encoder/Encoder_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * C 版本 Lossless 通用钩子定义：
 * - 通过 ctx + 函数指针模拟 C++ LosslessInterface 多态。
 * - 该结构应由具体 lossless（如 zstd/bypass）提供全局实例并注册到压缩器。
 */
typedef struct SZ3_LosslessOps_C {
    size_t (*compress)(const void *ctx, const sz3_uchar *src, size_t src_size, sz3_uchar *dst, size_t dst_cap);
    size_t (*decompress)(const void *ctx, const sz3_uchar *cmp_data, size_t cmp_size, sz3_uchar **buffer,
                         size_t *buffer_size);
} SZ3_LosslessOps_C;

#ifdef __cplusplus
}
#endif

#endif
