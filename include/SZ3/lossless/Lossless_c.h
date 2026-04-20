#ifndef SZ3_LOSSLESS_C_H
#define SZ3_LOSSLESS_C_H

#include <stddef.h>

#include "SZ3/encoder/Encoder_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明：用于在 ops 钩子中使用强类型 ctx。 */
typedef struct SZ3_LosslessCtx_C SZ3_LosslessCtx_C;

/*
 * C 版本 Lossless 通用钩子定义：
 * - 通过强类型 ctx + 函数指针模拟 C++ LosslessInterface 多态。
 * - 该结构应由具体 lossless（如 zstd/bypass）提供全局实例并注册到压缩器。
 */
typedef struct SZ3_LosslessOps_C {
    /*
     * init 钩子用于统一初始化不同 lossless 模块（如 zstd/bypass）：
     * - zstd: 可在此处完成压缩级别等参数的兜底初始化；
     * - bypass: 提供空实现以保证接口兼容。
     */
    int (*init)(SZ3_LosslessCtx_C *ctx);
    size_t (*compress)(const SZ3_LosslessCtx_C *ctx, const sz3_uchar *src, size_t src_size, sz3_uchar *dst,
                       size_t dst_cap);
    size_t (*decompress)(const SZ3_LosslessCtx_C *ctx, const sz3_uchar *cmp_data, size_t cmp_size, sz3_uchar **buffer,
                         size_t *buffer_size);
} SZ3_LosslessOps_C;

/*
 * 对外公开的 lossless 通用上下文：
 * - 当前用于跨模块传递“最小公共配置”（例如 compression_level）；
 * - 具体模块可在自身 ctx 中嵌入该结构，或按需扩展额外字段。
 */
struct SZ3_LosslessCtx_C {
    int compression_level;
};

#ifdef __cplusplus
}
#endif

#endif
