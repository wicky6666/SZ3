#ifndef SZ3_ENCODER_C_H
#define SZ3_ENCODER_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char sz3_uchar;

/*
 * C 版本 Encoder 通用钩子定义（i32 专用）：
 * - 通过 ctx + 函数指针模拟 C++ 多态。
 * - 该结构应由具体 encoder（如 Huffman i32）提供全局实例并注册到压缩器。
 */
typedef struct SZ3_EncoderOps_i32_C {
    int (*preprocess_encode)(void *ctx, const int *quant_inds, size_t quant_size, int out_range_end);
    size_t (*size_est)(const void *ctx);
    void (*save)(const void *ctx, sz3_uchar **buffer_pos);
    size_t (*encode)(const void *ctx, const int *quant_inds, size_t quant_size, sz3_uchar **buffer_pos);
    void (*postprocess_encode)(void *ctx);
    int (*load)(void *ctx, const sz3_uchar **buffer_pos, size_t *remaining_length);
    int (*decode)(void *ctx, const sz3_uchar **buffer_pos, size_t quant_size, int *quant_inds_out);
    void (*postprocess_decode)(void *ctx);
    void (*destroy)(void *ctx);
} SZ3_EncoderOps_i32_C;

#ifdef __cplusplus
}
#endif

#endif
