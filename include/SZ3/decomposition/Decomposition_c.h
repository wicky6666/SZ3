#ifndef SZ3_DECOMPOSITION_C_H
#define SZ3_DECOMPOSITION_C_H

#include <stddef.h>

#include "SZ3/encoder/Encoder_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明：具体定义由 SZGenericCompressor_c.h 提供。 */
typedef struct SZ3_Config_C SZ3_Config_C;

/*
 * C 版本 Decomposition 通用钩子定义：
 * - 通过 ctx + 函数指针模拟 C++ DecompositionInterface 多态。
 * - 该结构应由具体 decomposition（如 interpolation）提供全局实例并注册到压缩器。
 */
typedef struct SZ3_DecompositionOps_C {
    int (*compress)(void *ctx, const SZ3_Config_C *conf, void *data, int **quant_inds, size_t *quant_size);
    void (*get_out_range)(void *ctx, int *out_begin, int *out_end);
    size_t (*size_est)(const void *ctx);
    void (*save)(const void *ctx, sz3_uchar **buffer_pos);
    int (*load)(void *ctx, const sz3_uchar **buffer_pos, size_t *remaining_length);
    int (*decompress)(void *ctx, const SZ3_Config_C *conf, const int *quant_inds, size_t quant_size, void *dec_data);
} SZ3_DecompositionOps_C;

#ifdef __cplusplus
}
#endif

#endif
