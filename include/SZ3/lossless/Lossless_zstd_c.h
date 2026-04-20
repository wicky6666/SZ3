#ifndef SZ3_LOSSLESS_ZSTD_C_H
#define SZ3_LOSSLESS_ZSTD_C_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "SZ3/lossless/Lossless_c.h"
#include "zstd.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Lossless_zstd.hpp 的 C 版上下文：
 * - 对外公开，便于上层直接配置 compression_level；
 * - 继承（组合）通用 common 字段，保持与其他 lossless 模块一致。
 */
typedef SZ3_LosslessCtx_C SZ3_LosslessZstd_C;

/* 默认初始化：与 C++ 版本保持一致，默认压缩级别为 3。 */
static inline void sz3_lossless_zstd_init_default(SZ3_LosslessZstd_C *ctx) {
    if (ctx == NULL) return;
    ctx->compression_level = 3;
}

/* 带压缩级别初始化：便于调用方显式指定 zstd level。 */
static inline void sz3_lossless_zstd_init_with_level(SZ3_LosslessZstd_C *ctx, int compression_level) {
    if (ctx == NULL) return;
    ctx->compression_level = compression_level;
}

/*
 * 通用 init 钩子：
 * - 关键步骤：若 compression_level 未设置（例如 0），兜底为默认值 3；
 * - 返回 0 表示成功。
 */
static inline int sz3_lossless_zstd_init(SZ3_LosslessCtx_C *ctx) {
    if (ctx == NULL) return -1;
    if (ctx->compression_level == 0) {
        ctx->compression_level = 3;
    }
    return 0;
}

/* 内部工具函数：写入原始长度头（与 C++ write(srcLen, dst) 对齐）。 */
static inline void sz3_lossless_zstd_write_size(size_t value, sz3_uchar *dst) {
    memcpy(dst, &value, sizeof(size_t));
}

/* 内部工具函数：读取原始长度头（与 C++ read(dstLen, src) 对齐）。 */
static inline void sz3_lossless_zstd_read_size(size_t *value, const sz3_uchar *src) {
    memcpy(value, src, sizeof(size_t));
}

/*
 * C 版 compress：逐函数对齐 Lossless_zstd.hpp::compress。
 * 未完整实现说明：
 * - C++ 版本在 dstCap 不足时抛异常；C 版本当前返回 0（建议后续接入统一错误码）。
 */
static inline size_t sz3_lossless_zstd_compress(const SZ3_LosslessCtx_C *ctx, const sz3_uchar *src, size_t src_size,
                                                sz3_uchar *dst, size_t dst_cap) {
    size_t dst_len;

    if (ctx == NULL || src == NULL || dst == NULL) return 0;
    if (dst_cap < sizeof(size_t)) return 0;

    /* 关键步骤1：先写入原始长度，解压时据此分配输出缓冲。 */
    sz3_lossless_zstd_write_size(src_size, dst);
    dst += sizeof(size_t);
    dst_cap -= sizeof(size_t);

    /*
     * 关键步骤2：严格检查容量，避免 zstd 在容量不足时“静默截断”输出。
     * 与 C++ 版本逻辑一致：要求 dst_cap >= ZSTD_compressBound(src_size)。
     */
    if (dst_cap < ZSTD_compressBound(src_size)) {
        return 0;
    }

    dst_len = ZSTD_compress(dst, dst_cap, src, src_size, ctx->compression_level);
    if (ZSTD_isError(dst_len)) {
        /* 未完整实现：当前仅返回 0，后续可映射为项目统一错误码。 */
        return 0;
    }
    return dst_len + sizeof(size_t);
}

/*
 * C 版 decompress：逐函数对齐 Lossless_zstd.hpp::decompress。
 * 未完整实现说明：
 * - C++ 版本直接返回 ZSTD_decompress 的结果；
 * - 这里补充了输入合法性与错误检查，失败时返回 0。
 */
static inline size_t sz3_lossless_zstd_decompress(const SZ3_LosslessCtx_C *ctx, const sz3_uchar *cmp_data, size_t cmp_size,
                                                  sz3_uchar **buffer, size_t *buffer_size) {
    (void)ctx;
    size_t dec_size;

    if (cmp_data == NULL || buffer == NULL || buffer_size == NULL) return 0;
    if (cmp_size < sizeof(size_t)) return 0;

    /* 关键步骤1：读取原始长度头，用于分配/校验输出缓冲。 */
    sz3_lossless_zstd_read_size(buffer_size, cmp_data);

    if (*buffer == NULL) {
        *buffer = (sz3_uchar *)malloc(*buffer_size);
        if (*buffer == NULL) return 0;
    }

    /* 关键步骤2：跳过长度头，调用 zstd 解压核心接口。 */
    dec_size = ZSTD_decompress(*buffer, *buffer_size, cmp_data + sizeof(size_t), cmp_size - sizeof(size_t));
    if (ZSTD_isError(dec_size)) {
        return 0;
    }
    return dec_size;
}

/* Lossless zstd 的通用配置全局实例。 */
static const SZ3_LosslessOps_C SZ3_LosslessZstd_Ops = {
    sz3_lossless_zstd_init,
    sz3_lossless_zstd_compress,
    sz3_lossless_zstd_decompress};

#ifdef __cplusplus
}
#endif

#endif
