#ifndef SZ3_COMPRESSOR_SZ_GENERIC_C_H
#define SZ3_COMPRESSOR_SZ_GENERIC_C_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "SZ3/decomposition/Decomposition_c.h"
#include "SZ3/encoder/Encoder_c.h"
#include "SZ3/lossless/Lossless_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 说明：
 * 1) 本文件是对 SZGenericCompressor.hpp 的 C 风格改写（函数级流程对齐）。
 * 2) 由于 C 不支持模板与类继承，这里使用“上下文指针 + 函数指针”模拟模块组合。
 * 3) Decomposition/Lossless 的通用钩子定义已拆分至对应 *_c.h。
 */

typedef struct SZ3_Config_C {
    /* TODO(未完整实现): 仅占位，具体字段由调用方按项目 Config 映射扩展 */
    size_t num;
} SZ3_Config_C;

typedef struct SZ3_GenericCompressor_C {
    void *decomposition_ctx;
    void *encoder_ctx;
    SZ3_LosslessCtx_C *lossless_ctx;

    SZ3_DecompositionOps_C decomposition_ops;
    SZ3_EncoderOps_i32_C encoder_ops;
    SZ3_LosslessOps_C lossless_ops;
} SZ3_GenericCompressor_C;

/* 内部工具函数：按 memcpy 写入 size_t */
static inline void sz3_write_size_t(size_t value, sz3_uchar **buffer_pos) {
    memcpy(*buffer_pos, &value, sizeof(size_t));
    *buffer_pos += sizeof(size_t);
}

/* 内部工具函数：按 memcpy 读取 size_t */
static inline void sz3_read_size_t(size_t *value, const sz3_uchar **buffer_pos) {
    memcpy(value, *buffer_pos, sizeof(size_t));
    *buffer_pos += sizeof(size_t);
}

/* 初始化通用压缩器（相当于 C++ 构造函数） */
static inline void sz3_generic_compressor_init(SZ3_GenericCompressor_C *c, void *decomposition_ctx,
                                               SZ3_DecompositionOps_C decomposition_ops, void *encoder_ctx,
                                               SZ3_EncoderOps_i32_C encoder_ops, SZ3_LosslessCtx_C *lossless_ctx,
                                               SZ3_LosslessOps_C lossless_ops) {
    c->decomposition_ctx = decomposition_ctx;
    c->encoder_ctx = encoder_ctx;
    c->lossless_ctx = lossless_ctx;
    c->decomposition_ops = decomposition_ops;
    c->encoder_ops = encoder_ops;
    c->lossless_ops = lossless_ops;

    /* 关键步骤：初始化 lossless，上层可无缝切换 zstd/bypass。 */
    if (c->lossless_ops.init) {
        c->lossless_ops.init(c->lossless_ctx);
    }
}

/* 注册 encoder 通用钩子（便于直接复用 encoder 模块中的全局 ops 实例）。 */
static inline void sz3_generic_compressor_register_encoder_ops(SZ3_GenericCompressor_C *c,
                                                               const SZ3_EncoderOps_i32_C *encoder_ops) {
    if (c == NULL || encoder_ops == NULL) return;
    c->encoder_ops = *encoder_ops;
}

/* 注册 decomposition 通用钩子（便于直接复用 decomposition 模块中的全局 ops 实例）。 */
static inline void sz3_generic_compressor_register_decomposition_ops(SZ3_GenericCompressor_C *c,
                                                                     const SZ3_DecompositionOps_C *decomposition_ops) {
    if (c == NULL || decomposition_ops == NULL) return;
    c->decomposition_ops = *decomposition_ops;
}

/* 注册 lossless 通用钩子（便于直接复用 lossless 模块中的全局 ops 实例）。 */
static inline void sz3_generic_compressor_register_lossless_ops(SZ3_GenericCompressor_C *c,
                                                                const SZ3_LosslessOps_C *lossless_ops) {
    if (c == NULL || lossless_ops == NULL) return;
    c->lossless_ops = *lossless_ops;
    /* 重新注册 lossless 时执行 init，确保新模块状态可用。 */
    if (c->lossless_ops.init) {
        c->lossless_ops.init(c->lossless_ctx);
    }
}

/*
 * C 版 compress：流程对齐 SZGenericCompressor.hpp::compress
 * 返回值：压缩后字节数；返回 0 表示失败或未完整实现分支。
 */
static inline size_t sz3_generic_compress(SZ3_GenericCompressor_C *c, const SZ3_Config_C *conf, void *data,
                                          sz3_uchar *cmp_data, size_t cmp_cap) {
    int *quant_inds = NULL;
    size_t quant_size = 0;
    int out_begin = 0, out_end = 0;

    /* 关键步骤1：前端分解，生成量化索引 */
    if (!c->decomposition_ops.compress ||
        c->decomposition_ops.compress(c->decomposition_ctx, conf, data, &quant_inds, &quant_size) != 0) {
        return 0;
    }

    if (!c->decomposition_ops.get_out_range) {
        /* 未完整实现：缺少 out range 获取函数 */
        return 0;
    }
    c->decomposition_ops.get_out_range(c->decomposition_ctx, &out_begin, &out_end);

    /* 与 C++ 版本一致：要求输出区间从 0 开始 */
    if (out_begin != 0) {
        /* 未完整实现：这里可替换为项目统一错误码/日志机制 */
        return 0;
    }

    if (!c->encoder_ops.preprocess_encode ||
        c->encoder_ops.preprocess_encode(c->encoder_ctx, quant_inds, quant_size, out_end) != 0) {
        return 0;
    }

    /* 关键步骤2：估算中间缓冲区大小（保持与 C++ 逻辑一致） */
    size_t decomp_est = c->decomposition_ops.size_est ? c->decomposition_ops.size_est(c->decomposition_ctx) : 0;
    size_t enc_est = c->encoder_ops.size_est ? c->encoder_ops.size_est(c->encoder_ctx) : 0;
    size_t buffer_size = 2 * (decomp_est + enc_est + sizeof(int) * quant_size);
    if (buffer_size < 1000) buffer_size = 1000;

    sz3_uchar *buffer = (sz3_uchar *)malloc(buffer_size);
    if (!buffer) return 0;
    sz3_uchar *buffer_pos = buffer;

    /* 关键步骤3：序列化 decomposition/encoder 状态 + quant size + 量化索引流 */
    if (c->decomposition_ops.save) c->decomposition_ops.save(c->decomposition_ctx, &buffer_pos);
    if (c->encoder_ops.save) c->encoder_ops.save(c->encoder_ctx, &buffer_pos);

    sz3_write_size_t(quant_size, &buffer_pos);

    if (!c->encoder_ops.encode || c->encoder_ops.encode(c->encoder_ctx, quant_inds, quant_size, &buffer_pos) == 0) {
        free(buffer);
        return 0;
    }

    if (c->encoder_ops.postprocess_encode) c->encoder_ops.postprocess_encode(c->encoder_ctx);

    /* 关键步骤4：无损压缩 */
    if (!c->lossless_ops.compress) {
        free(buffer);
        return 0;
    }
    size_t cmp_size = c->lossless_ops.compress(c->lossless_ctx, buffer, (size_t)(buffer_pos - buffer), cmp_data, cmp_cap);

    free(buffer);
    return cmp_size;
}

/*
 * C 版 decompress：流程对齐 SZGenericCompressor.hpp::decompress
 * 返回值：0 成功，非 0 失败。
 */
static inline int sz3_generic_decompress(SZ3_GenericCompressor_C *c, const SZ3_Config_C *conf,
                                         const sz3_uchar *cmp_data, size_t cmp_size, void *dec_data) {
    sz3_uchar *buffer = NULL;
    size_t buffer_size = 0;

    /* 关键步骤1：先做无损解压，恢复中间缓冲区 */
    if (!c->lossless_ops.decompress ||
        c->lossless_ops.decompress(c->lossless_ctx, cmp_data, cmp_size, &buffer, &buffer_size) == 0) {
        return -1;
    }

    const sz3_uchar *buffer_pos = buffer;

    /* 关键步骤2：反序列化模块状态 */
    size_t remaining_length = buffer_size;
    if ((c->decomposition_ops.load && c->decomposition_ops.load(c->decomposition_ctx, &buffer_pos, &remaining_length) != 0) ||
        (c->encoder_ops.load && c->encoder_ops.load(c->encoder_ctx, &buffer_pos, &remaining_length) != 0)) {
        free(buffer);
        return -1;
    }

    /* 关键步骤3：读取 quant size 并解码量化索引 */
    size_t quant_size = 0;
    sz3_read_size_t(&quant_size, &buffer_pos);

    int *quant_inds = NULL;
    if (quant_size > 0) {
        quant_inds = (int *)malloc(sizeof(int) * quant_size);
        if (quant_inds == NULL) {
            free(buffer);
            return -1;
        }
    }
    if (!c->encoder_ops.decode || c->encoder_ops.decode(c->encoder_ctx, &buffer_pos, quant_size, quant_inds) != 0) {
        free(quant_inds);
        free(buffer);
        return -1;
    }

    if (c->encoder_ops.postprocess_decode) c->encoder_ops.postprocess_decode(c->encoder_ctx);

    free(buffer);

    /* 关键步骤4：前端重建原始数据 */
    if (!c->decomposition_ops.decompress ||
        c->decomposition_ops.decompress(c->decomposition_ctx, conf, quant_inds, quant_size, dec_data) != 0) {
        free(quant_inds);
        return -1;
    }

    free(quant_inds);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif
