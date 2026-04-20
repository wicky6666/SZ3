#ifndef SZ3_LOSSLESS_BYPASS_C_H
#define SZ3_LOSSLESS_BYPASS_C_H

#include <stdlib.h>
#include <string.h>

#include "SZ3/lossless/Lossless_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* bypass 无额外状态，直接复用通用 ctx 强类型。 */
typedef SZ3_LosslessCtx_C SZ3_LosslessBypass_C;

/* bypass 的 init 为空实现：仅用于满足通用 init 钩子接口。 */
static inline int sz3_lossless_bypass_init(SZ3_LosslessCtx_C *ctx) {
    (void)ctx;
    return 0;
}

static inline size_t sz3_lossless_bypass_compress(const SZ3_LosslessCtx_C *ctx, const sz3_uchar *src, size_t src_size,
                                                  sz3_uchar *dst, size_t dst_cap) {
    (void)ctx;
    if (src == NULL || dst == NULL || dst_cap < src_size) return 0;
    memcpy(dst, src, src_size);
    return src_size;
}

static inline size_t sz3_lossless_bypass_decompress(const SZ3_LosslessCtx_C *ctx, const sz3_uchar *cmp_data, size_t cmp_size,
                                                    sz3_uchar **buffer, size_t *buffer_size) {
    (void)ctx;
    if (cmp_data == NULL || buffer == NULL || buffer_size == NULL) return 0;
    *buffer_size = cmp_size;
    if (*buffer == NULL) {
        *buffer = (sz3_uchar *)malloc(*buffer_size);
        if (*buffer == NULL) return 0;
    }
    memcpy(*buffer, cmp_data, *buffer_size);
    return *buffer_size;
}

/* Lossless bypass 的通用配置全局实例。 */
static const SZ3_LosslessOps_C SZ3_LosslessBypass_Ops = {
    sz3_lossless_bypass_init,
    sz3_lossless_bypass_compress,
    sz3_lossless_bypass_decompress};

#ifdef __cplusplus
}
#endif

#endif
