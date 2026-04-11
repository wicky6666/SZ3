#ifndef SZ3_INTERPOLATION_DECOMPOSITION_C_H
#define SZ3_INTERPOLATION_DECOMPOSITION_C_H

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "SZ3/decomposition/Decomposition_c.h"
#include "SZ3/quantizer/line_quantizer_c.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ===================== 非编码模块依赖（空函数占位） =====================
 * 说明：
 * 1) 下列函数来自“插值算法外部依赖”能力（如遍历器、序列化工具、配置桥接等）。
 * 2) 目前按需求先给出空函数/空实现，便于后续逐步接入真实模块。
 */
/* 未完整实现：计时模块占位，当前不记录任何时间信息。 */
static inline void sz3_interp_dep_timer_start(const char *tag) {(void)tag;}
/* 未完整实现：计时模块占位，当前不记录任何时间信息。 */
static inline void sz3_interp_dep_timer_stop(const char *tag) {(void)tag;}
/* 未完整实现：序列化写接口占位，当前不向输出缓冲写入字节。 */
static inline void sz3_interp_dep_write_bytes(const void *src, size_t n, unsigned char **c) {
    (void)src;
    (void)n;
    (void)c;
}
/* 未完整实现：序列化读接口占位，当前不从输入缓冲读取字节。 */
static inline void sz3_interp_dep_read_bytes(void *dst, size_t n, const unsigned char **c, size_t *remaining) {
    (void)dst;
    (void)n;
    (void)c;
    (void)remaining;
}
/* 未完整实现：N 维 foreach 遍历器占位，当前不执行任何遍历。 */
static inline void sz3_interp_dep_foreach_nd_placeholder(void) {}

/*
 * ===================== 常量与基础结构体 =====================
 */
#define SZ3_INTERP_MAX_DIMS 4u

typedef struct SZ3InterpConfigC {
    /* 维度数量（仅支持 1~4） */
    uint32_t num_dims;
    /* 各维长度 */
    size_t dims[SZ3_INTERP_MAX_DIMS];
    /* 插值算法：0=linear, 1=cubic */
    int interp_algo;
    /* 方向排列编号（对应置换序号） */
    int interp_direction;
    /* 锚点步长（0 表示不启用锚点） */
    size_t interp_anchor_stride;
    /* 分层误差参数 */
    double interp_alpha;
    double interp_beta;
} SZ3InterpConfigC;

typedef struct SZ3InterpolationDecompositionC {
    /* 基础配置 */
    uint32_t n;
    int interp_level;
    int interp_id;
    uint32_t blocksize;
    int direction_sequence_id;

    /* 量化索引缓冲 */
    int *quant_inds;
    size_t quant_index;
    size_t num_elements;

    /* 尺寸与偏移 */
    size_t original_dimensions[SZ3_INTERP_MAX_DIMS];
    size_t original_dim_offsets[SZ3_INTERP_MAX_DIMS];

    /* 锚点与误差参数 */
    size_t anchor_stride;
    double eb_alpha;
    double eb_beta;
    double eb_ratio;

    /* 运行时：保存维度顺序集合（最多 4! = 24） */
    int dim_sequences[24][SZ3_INTERP_MAX_DIMS];
    int dim_sequence_count;

    /* 量化器：当前仅 float */
    SZ3LineQuantizerC quantizer;
} SZ3InterpolationDecompositionC;

/*
 * ===================== 对外函数声明（按 C++ 成员函数一一映射） =====================
 */

/* 构造/初始化：对应 C++ 构造函数 */
static inline void sz3_interp_decomp_init(SZ3InterpolationDecompositionC *ctx,
                                          const SZ3InterpConfigC *conf,
                                          const SZ3LineQuantizerC *quantizer_template);

/* 释放运行资源 */
static inline void sz3_interp_decomp_destroy(SZ3InterpolationDecompositionC *ctx);

/* 压缩：对应 compress（仅 float） */
static inline int *sz3_interp_decomp_compress(SZ3InterpolationDecompositionC *ctx,
                                              const SZ3InterpConfigC *conf,
                                              float *data,
                                              size_t *out_count);

/* 解压：对应 decompress（仅 float） */
static inline float *sz3_interp_decomp_decompress(SZ3InterpolationDecompositionC *ctx,
                                                  const SZ3InterpConfigC *conf,
                                                  int *quant_inds,
                                                  float *dec_data,
                                                  size_t quant_count);

/* 序列化/反序列化：对应 save/load */
static inline void sz3_interp_decomp_save(SZ3InterpolationDecompositionC *ctx, unsigned char **c);
static inline void sz3_interp_decomp_load(SZ3InterpolationDecompositionC *ctx,
                                          const unsigned char **c,
                                          size_t *remaining_length);

/* 输出范围：对应 get_out_range */
static inline SZ3RangeI32 sz3_interp_decomp_get_out_range(const SZ3InterpolationDecompositionC *ctx);

/*
 * ===================== 内部函数声明（对应 C++ private 方法） =====================
 */
static inline void sz3_interp_decomp_init_runtime(SZ3InterpolationDecompositionC *ctx);
static inline void sz3_interp_decomp_build_anchor_grid(SZ3InterpolationDecompositionC *ctx, float *data);
static inline void sz3_interp_decomp_recover_anchor_grid(SZ3InterpolationDecompositionC *ctx, float *data);

/* 线性插值：使用左右邻点的平均值。 */
static inline float sz3_interp_linear(float a, float b) { return (a + b) * 0.5f; }
/* 线性外推：常用于边界位置，按最近两个点估计下一个点。 */
static inline float sz3_interp_linear1(float a, float b) { return -0.5f * a + 1.5f * b; }
/* 二次插值模板 1：用于靠左区域的三点插值。 */
static inline float sz3_interp_quad_1(float a, float b, float c) { return (3.0f * a + 6.0f * b - c) * 0.125f; }
/* 二次插值模板 2：用于中间区域的三点插值。 */
static inline float sz3_interp_quad_2(float a, float b, float c) { return (-a + 6.0f * b + 3.0f * c) * 0.125f; }
/* 二次插值模板 3：用于靠右区域的三点插值。 */
static inline float sz3_interp_quad_3(float a, float b, float c) { return (3.0f * a - 10.0f * b + 15.0f * c) / 8.0f; }
/* 三次插值：四点模板，中心点精度通常高于线性/二次。 */
static inline float sz3_interp_cubic(float a, float b, float c, float d) {
    return (-a + 9.0f * b + 9.0f * c - d) * 0.0625f;
}

/* C 版本中将模板回调展开为函数指针。 */
typedef void (*sz3_interp_quantize_cb)(size_t idx, float *d, float pred, void *user_data);

static inline double sz3_interp_decomp_interpolation_1d(SZ3InterpolationDecompositionC *ctx,
                                                        float *data,
                                                        size_t begin,
                                                        size_t end,
                                                        size_t stride,
                                                        int interp_id,
                                                        sz3_interp_quantize_cb cb,
                                                        void *user_data);

/* 对应 C++ interpolation_1d_fastest_dim_first：当前先提供空函数占位。 */
static inline double sz3_interp_decomp_interpolation_1d_fastest_dim_first(
    SZ3InterpolationDecompositionC *ctx,
    float *data,
    const size_t begin_idx[SZ3_INTERP_MAX_DIMS],
    const size_t end_idx[SZ3_INTERP_MAX_DIMS],
    size_t direction,
    size_t strides[SZ3_INTERP_MAX_DIMS],
    size_t math_stride,
    int interp_id,
    sz3_interp_quantize_cb cb,
    void *user_data);

/* 对应 C++ interpolation：当前先提供空函数占位。 */
static inline double sz3_interp_decomp_interpolation(SZ3InterpolationDecompositionC *ctx,
                                                     float *data,
                                                     const size_t begin[SZ3_INTERP_MAX_DIMS],
                                                     const size_t end[SZ3_INTERP_MAX_DIMS],
                                                     int interp_id,
                                                     sz3_interp_quantize_cb cb,
                                                     int direction,
                                                     size_t stride,
                                                     void *user_data);

typedef struct SZ3InterpQuantizeCtxC {
    /* 回调里通过 user_data 回传主上下文，避免全局变量。 */
    SZ3InterpolationDecompositionC *ctx;
} SZ3InterpQuantizeCtxC;

static inline void sz3_interp_quantize_and_overwrite_cb(size_t idx, float *d, float pred, void *user_data);

typedef struct SZ3InterpRecoverCtxC {
    /* quant_count 用于防越界：恢复时不能读超过量化索引总数。 */
    SZ3InterpolationDecompositionC *ctx;
    size_t quant_count;
} SZ3InterpRecoverCtxC;

static inline void sz3_interp_recover_cb(size_t idx, float *d, float pred, void *user_data);

/*
 * ===================== 内联实现 =====================
 */

static inline void sz3_interp_decomp_init(SZ3InterpolationDecompositionC *ctx,
                                          const SZ3InterpConfigC *conf,
                                          const SZ3LineQuantizerC *quantizer_template) {
    uint32_t i;
    if (ctx == NULL || conf == NULL || quantizer_template == NULL) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    /* n 仅允许 1~4，超出上限时在 C 层强制截断。 */
    ctx->n = conf->num_dims;
    if (ctx->n > SZ3_INTERP_MAX_DIMS) {
        ctx->n = SZ3_INTERP_MAX_DIMS;
    }
    ctx->interp_id = conf->interp_algo;
    ctx->direction_sequence_id = conf->interp_direction;
    ctx->anchor_stride = conf->interp_anchor_stride;
    ctx->eb_alpha = conf->interp_alpha;
    ctx->eb_beta = conf->interp_beta;
    ctx->eb_ratio = 0.5;
    /* blocksize 需保持偶数，便于每层按 2 分裂插值。 */
    ctx->blocksize = 32u;
    ctx->interp_level = -1;
    for (i = 0; i < ctx->n && i < SZ3_INTERP_MAX_DIMS; i++) {
        ctx->original_dimensions[i] = conf->dims[i];
    }
    ctx->quantizer = *quantizer_template;
    /* 深拷贝时不共享 unpred 缓冲，避免双重释放。 */
    ctx->quantizer.unpred = NULL;
    ctx->quantizer.unpred_size = 0;
    ctx->quantizer.unpred_capacity = 0;
    ctx->quantizer.index = 0;
}

static inline void sz3_interp_decomp_destroy(SZ3InterpolationDecompositionC *ctx) {
    if (ctx == NULL) {
        return;
    }
    sz3_line_quantizer_destroy(&ctx->quantizer);
}

static inline void sz3_interp_decomp_init_runtime(SZ3InterpolationDecompositionC *ctx) {
    uint32_t i;
    int used[SZ3_INTERP_MAX_DIMS] = {0, 0, 0, 0};
    int perm[SZ3_INTERP_MAX_DIMS] = {0, 0, 0, 0};
    int top = 0;
    int next_choice[SZ3_INTERP_MAX_DIMS] = {0, 0, 0, 0};
    int produced = 0;
    int use_anchor = 0;
    if (ctx == NULL || ctx->n == 0 || ctx->n > SZ3_INTERP_MAX_DIMS) {
        return;
    }
    ctx->quant_index = 0;
    /* num_elements 用于一次性分配 quant_inds，避免循环中多次扩容。 */
    ctx->num_elements = 1;
    ctx->interp_level = -1;

    assert((ctx->blocksize % 2u) == 0u);
    assert((ctx->anchor_stride == 0u) || ((ctx->anchor_stride & (ctx->anchor_stride - 1u)) == 0u));

    for (i = 0; i < ctx->n; i++) {
        size_t d = ctx->original_dimensions[i];
        /* 每一维的 level=ceil(log2(dim))，最终取最大层数作为全局插值层级。 */
        int level = (d <= 1) ? 0 : (int)ceil(log2((double)d));
        if (level > ctx->interp_level) {
            ctx->interp_level = level;
        }
        if (ctx->anchor_stride > 0 && d > ctx->anchor_stride) {
            use_anchor = 1;
        }
        ctx->num_elements *= d;
    }
    if (!use_anchor) {
        ctx->anchor_stride = 0;
    }
    if (ctx->anchor_stride > 0) {
        int max_interpolation_level = (int)log2((double)ctx->anchor_stride) + 1;
        if (max_interpolation_level <= ctx->interp_level) {
            ctx->interp_level = max_interpolation_level;
        }
    }

    ctx->original_dim_offsets[ctx->n - 1] = 1;
    /* 生成行主序 offset：offset[i]=prod_{j>i}(dim[j])。 */
    for (i = (uint32_t)(ctx->n - 1); i > 0; i--) {
        ctx->original_dim_offsets[i - 1] = ctx->original_dim_offsets[i] * ctx->original_dimensions[i];
    }

    /* 生成与 C++ 实现一致的维度顺序全排列（最多 4! = 24）。 */
    while (top >= 0) {
        int c;
        if (top == (int)ctx->n) {
            for (i = 0; i < ctx->n; i++) {
                ctx->dim_sequences[produced][i] = perm[i];
            }
            produced++;
            top--;
            if (top >= 0) {
                used[perm[top]] = 0;
            }
            continue;
        }
        c = next_choice[top];
        while (c < (int)ctx->n && used[c]) {
            c++;
        }
        if (c < (int)ctx->n) {
            next_choice[top] = c + 1;
            perm[top] = c;
            used[c] = 1;
            top++;
            if (top < (int)ctx->n) {
                next_choice[top] = 0;
            }
        } else {
            next_choice[top] = 0;
            top--;
            if (top >= 0) {
                used[perm[top]] = 0;
            }
        }
    }
    ctx->dim_sequence_count = produced;
}

static inline void sz3_interp_quantize_and_overwrite_cb(size_t idx, float *d, float pred, void *user_data) {
    SZ3InterpQuantizeCtxC *qctx = (SZ3InterpQuantizeCtxC *)user_data;
    if (qctx == NULL || qctx->ctx == NULL || d == NULL) {
        return;
    }
    /* quant_index 在此单调递增，写入顺序即后续解压读取顺序。 */
    qctx->ctx->quant_inds[qctx->ctx->quant_index++] =
        sz3_line_quantizer_quantize_and_overwrite(&qctx->ctx->quantizer, d, pred);
    (void)idx;
}

static inline void sz3_interp_recover_cb(size_t idx, float *d, float pred, void *user_data) {
    SZ3InterpRecoverCtxC *rctx = (SZ3InterpRecoverCtxC *)user_data;
    if (rctx == NULL || rctx->ctx == NULL || d == NULL) {
        return;
    }
    /* 关键步骤：按与压缩一致的顺序读取量化索引并执行反量化恢复。 */
    if (rctx->ctx->quant_index < rctx->quant_count) {
        *d = sz3_line_quantizer_recover(&rctx->ctx->quantizer, pred, rctx->ctx->quant_inds[rctx->ctx->quant_index++]);
    } else {
        /* 未完整实现：当 quant_count 不足时，当前仅保持预测值，避免越界读。 */
        *d = pred;
    }
    (void)idx;
}

static inline void sz3_interp_decomp_build_anchor_grid(SZ3InterpolationDecompositionC *ctx, float *data) {
    (void)ctx;
    (void)data;
    /* 未完整实现：依赖 foreach N 维遍历器，尚未落地锚点采样与量化写入。 */
    sz3_interp_dep_foreach_nd_placeholder();
}

static inline void sz3_interp_decomp_recover_anchor_grid(SZ3InterpolationDecompositionC *ctx, float *data) {
    (void)ctx;
    (void)data;
    /* 未完整实现：依赖 foreach N 维遍历器，尚未落地锚点恢复与游标推进。 */
    sz3_interp_dep_foreach_nd_placeholder();
}

static inline double sz3_interp_decomp_interpolation_1d(SZ3InterpolationDecompositionC *ctx,
                                                        float *data,
                                                        size_t begin,
                                                        size_t end,
                                                        size_t stride,
                                                        int interp_id,
                                                        sz3_interp_quantize_cb cb,
                                                        void *user_data) {
    size_t n, i;
    size_t stride3x, stride5x;
    (void)ctx;
    if (data == NULL || cb == NULL || stride == 0) {
        return 0.0;
    }

    n = (end - begin) / stride + 1;
    /* n 表示当前 1D 片段采样点数，不足 2 个点则无法插值。 */
    if (n <= 1) {
        return 0.0;
    }

    stride3x = 3 * stride;
    stride5x = 5 * stride;

    if (interp_id == 0 || n < 5) {
        /* 线性模式或短序列：按“奇数位”补点。 */
        for (i = 1; i + 1 < n; i += 2) {
            float *d = data + begin + i * stride;
            cb((size_t)(d - data), d, sz3_interp_linear(*(d - stride), *(d + stride)), user_data);
        }
        if ((n % 2) == 0) {
            /* 偶数长度时尾点没有右邻居，改用边界外推策略。 */
            float *d = data + begin + (n - 1) * stride;
            if (n < 4) {
                cb((size_t)(d - data), d, *(d - stride), user_data);
            } else {
                cb((size_t)(d - data), d, sz3_interp_linear1(*(d - stride3x), *(d - stride)), user_data);
            }
        }
    } else {
        /* 三次模式：中间点走 cubic，首尾邻域走二次模板。 */
        for (i = 3; i + 3 < n; i += 2) {
            float *d = data + begin + i * stride;
            cb((size_t)(d - data), d,
               sz3_interp_cubic(*(d - stride3x), *(d - stride), *(d + stride), *(d + stride3x)), user_data);
        }
        {
            float *d = data + begin + stride;
            cb((size_t)(d - data), d, sz3_interp_quad_1(*(d - stride), *(d + stride), *(d + stride3x)), user_data);
        }
        {
            float *d = data + begin + i * stride;
            cb((size_t)(d - data), d, sz3_interp_quad_2(*(d - stride3x), *(d - stride), *(d + stride)), user_data);
        }
        if ((n % 2) == 0) {
            float *d = data + begin + (n - 1) * stride;
            cb((size_t)(d - data), d,
               sz3_interp_quad_3(*(d - stride5x), *(d - stride3x), *(d - stride)), user_data);
        }
    }
    return 0.0;
}

/* 未完整实现：对应 C++ interpolation_1d_fastest_dim_first，当前返回 0 作为占位。 */
static inline double sz3_interp_decomp_interpolation_1d_fastest_dim_first(
    SZ3InterpolationDecompositionC *ctx,
    float *data,
    const size_t begin_idx[SZ3_INTERP_MAX_DIMS],
    const size_t end_idx[SZ3_INTERP_MAX_DIMS],
    size_t direction,
    size_t strides[SZ3_INTERP_MAX_DIMS],
    size_t math_stride,
    int interp_id,
    sz3_interp_quantize_cb cb,
    void *user_data) {
    (void)ctx;
    (void)data;
    (void)begin_idx;
    (void)end_idx;
    (void)direction;
    (void)strides;
    (void)math_stride;
    (void)interp_id;
    (void)cb;
    (void)user_data;
    return 0.0;
}

/* 未完整实现：对应 C++ interpolation（含 1D/2D/3D/4D 调度），当前返回 0 作为占位。 */
static inline double sz3_interp_decomp_interpolation(SZ3InterpolationDecompositionC *ctx,
                                                     float *data,
                                                     const size_t begin[SZ3_INTERP_MAX_DIMS],
                                                     const size_t end[SZ3_INTERP_MAX_DIMS],
                                                     int interp_id,
                                                     sz3_interp_quantize_cb cb,
                                                     int direction,
                                                     size_t stride,
                                                     void *user_data) {
    if (ctx == NULL || data == NULL || begin == NULL || end == NULL || cb == NULL || stride == 0) {
        return 0.0;
    }
    if (ctx->n == 1) {
        return sz3_interp_decomp_interpolation_1d(ctx, data, begin[0], end[0], stride, interp_id, cb, user_data);
    }
    /* 暂未处理 2D/3D/4D 分支；后续可按 C++ 原实现补齐维序遍历与 fastest-dim 调度逻辑。 */
    (void)direction;
    return 0.0;
}

static inline SZ3RangeI32 sz3_interp_decomp_get_out_range(const SZ3InterpolationDecompositionC *ctx) {
    if (ctx == NULL) {
        SZ3RangeI32 r = {0, 0};
        return r;
    }
    return sz3_line_quantizer_get_out_range(&ctx->quantizer);
}

static inline void sz3_interp_decomp_save(SZ3InterpolationDecompositionC *ctx, unsigned char **c) {
    if (ctx == NULL || c == NULL) {
        return;
    }
    sz3_interp_dep_write_bytes(ctx->original_dimensions, sizeof(size_t) * ctx->n, c);
    sz3_interp_dep_write_bytes(&ctx->blocksize, sizeof(ctx->blocksize), c);
    sz3_interp_dep_write_bytes(&ctx->interp_id, sizeof(ctx->interp_id), c);
    sz3_interp_dep_write_bytes(&ctx->direction_sequence_id, sizeof(ctx->direction_sequence_id), c);
    sz3_interp_dep_write_bytes(&ctx->anchor_stride, sizeof(ctx->anchor_stride), c);
    sz3_interp_dep_write_bytes(&ctx->eb_alpha, sizeof(ctx->eb_alpha), c);
    sz3_interp_dep_write_bytes(&ctx->eb_beta, sizeof(ctx->eb_beta), c);
    sz3_line_quantizer_save(&ctx->quantizer, c);
}

static inline void sz3_interp_decomp_load(SZ3InterpolationDecompositionC *ctx,
                                          const unsigned char **c,
                                          size_t *remaining_length) {
    if (ctx == NULL || c == NULL || remaining_length == NULL) {
        return;
    }
    sz3_interp_dep_read_bytes(ctx->original_dimensions, sizeof(size_t) * ctx->n, c, remaining_length);
    sz3_interp_dep_read_bytes(&ctx->blocksize, sizeof(ctx->blocksize), c, remaining_length);
    sz3_interp_dep_read_bytes(&ctx->interp_id, sizeof(ctx->interp_id), c, remaining_length);
    sz3_interp_dep_read_bytes(&ctx->direction_sequence_id, sizeof(ctx->direction_sequence_id), c, remaining_length);
    sz3_interp_dep_read_bytes(&ctx->anchor_stride, sizeof(ctx->anchor_stride), c, remaining_length);
    sz3_interp_dep_read_bytes(&ctx->eb_alpha, sizeof(ctx->eb_alpha), c, remaining_length);
    sz3_interp_dep_read_bytes(&ctx->eb_beta, sizeof(ctx->eb_beta), c, remaining_length);
    sz3_line_quantizer_load(&ctx->quantizer, c, remaining_length);
}

/*
 * 压缩/解压主流程：当前保留 C++ 同名函数的流程骨架，后续逐步补齐多维块遍历细节。
 * 由于用户当前要求是“先设计结构体和函数声明”，这里给出可编译的 C 版本框架。
 */
static inline int *sz3_interp_decomp_compress(SZ3InterpolationDecompositionC *ctx,
                                              const SZ3InterpConfigC *conf,
                                              float *data,
                                              size_t *out_count) {
    double eb;
    double cur_eb;
    int level;
    size_t dims_begin[SZ3_INTERP_MAX_DIMS] = {0, 0, 0, 0};
    size_t dims_end[SZ3_INTERP_MAX_DIMS] = {0, 0, 0, 0};
    SZ3InterpQuantizeCtxC qctx;
    if (ctx == NULL || out_count == NULL || data == NULL) {
        return NULL;
    }
    if (conf != NULL) {
        ctx->interp_id = conf->interp_algo;
        ctx->direction_sequence_id = conf->interp_direction;
        ctx->anchor_stride = conf->interp_anchor_stride;
        ctx->eb_alpha = conf->interp_alpha;
        ctx->eb_beta = conf->interp_beta;
        ctx->n = conf->num_dims;
        if (ctx->n > SZ3_INTERP_MAX_DIMS) {
            ctx->n = SZ3_INTERP_MAX_DIMS;
        }
        memcpy(ctx->original_dimensions, conf->dims, sizeof(size_t) * ctx->n);
    }

    sz3_interp_decomp_init_runtime(ctx);
    eb = sz3_line_quantizer_get_eb(&ctx->quantizer);
    /* 每次压缩都重建 quant_inds，避免复用旧缓冲造成脏数据。 */
    free(ctx->quant_inds);
    ctx->quant_inds = NULL;
    ctx->quant_inds = (int *)malloc(ctx->num_elements * sizeof(int));
    if (ctx->quant_inds == NULL) {
        *out_count = 0;
        return NULL;
    }

    if (ctx->anchor_stride == 0) {
        /* 无锚点模式：首样本以 pred=0 单独编码，作为后续插值基准。 */
        ctx->quant_inds[ctx->quant_index++] = sz3_line_quantizer_quantize_and_overwrite(&ctx->quantizer, data, 0.0f);
    } else {
        sz3_interp_decomp_build_anchor_grid(ctx, data);
        ctx->interp_level--;
    }

    qctx.ctx = ctx;
    for (level = ctx->interp_level; level > 0; level--) {
        size_t stride = ((size_t)1) << ((size_t)level - 1u);
        /* 当前层步长 stride=2^(level-1)，block 大小随层级同步放大。 */
        size_t interp_block_size = (size_t)ctx->blocksize * stride;
        size_t block_begin;
        cur_eb = eb;
        if (ctx->eb_alpha < 0) {
            cur_eb = (level >= 3) ? (eb * ctx->eb_ratio) : eb;
        } else if (ctx->eb_alpha >= 1) {
            double cur_ratio = pow(ctx->eb_alpha, (double)level - 1.0);
            if (cur_ratio > ctx->eb_beta) {
                cur_ratio = ctx->eb_beta;
            }
            cur_eb = eb / cur_ratio;
        }
        sz3_line_quantizer_set_eb(&ctx->quantizer, cur_eb);

        if (ctx->n != 1) {
            /* 暂未处理多维块遍历压缩。 */
            continue;
        }
        for (block_begin = 0; block_begin < ctx->original_dimensions[0]; block_begin += interp_block_size) {
            /* 每个 block 以 [begin,end] 闭区间送入 interpolation。 */
            dims_begin[0] = block_begin;
            dims_end[0] = block_begin + interp_block_size;
            if (dims_end[0] > ctx->original_dimensions[0] - 1) {
                dims_end[0] = ctx->original_dimensions[0] - 1;
            }
            sz3_interp_decomp_interpolation(ctx, data, dims_begin, dims_end, ctx->interp_id,
                                            sz3_interp_quantize_and_overwrite_cb, ctx->direction_sequence_id, stride,
                                            &qctx);
        }
    }
    sz3_line_quantizer_set_eb(&ctx->quantizer, eb);
    *out_count = ctx->num_elements;
    return ctx->quant_inds;
}

static inline float *sz3_interp_decomp_decompress(SZ3InterpolationDecompositionC *ctx,
                                                  const SZ3InterpConfigC *conf,
                                                  int *quant_inds,
                                                  float *dec_data,
                                                  size_t quant_count) {
    double eb;
    int level;
    size_t dims_begin[SZ3_INTERP_MAX_DIMS] = {0, 0, 0, 0};
    size_t dims_end[SZ3_INTERP_MAX_DIMS] = {0, 0, 0, 0};
    SZ3InterpRecoverCtxC rctx;
    if (ctx == NULL) {
        return NULL;
    }
    if (quant_inds == NULL || dec_data == NULL) {
        return NULL;
    }

    if (conf != NULL) {
        /* 与压缩路径同步更新运行参数，确保同一配置下可正确反量化。 */
        ctx->interp_id = conf->interp_algo;
        ctx->direction_sequence_id = conf->interp_direction;
        ctx->anchor_stride = conf->interp_anchor_stride;
        ctx->eb_alpha = conf->interp_alpha;
        ctx->eb_beta = conf->interp_beta;
        ctx->n = conf->num_dims;
        if (ctx->n > SZ3_INTERP_MAX_DIMS) {
            ctx->n = SZ3_INTERP_MAX_DIMS;
        }
        memcpy(ctx->original_dimensions, conf->dims, sizeof(size_t) * ctx->n);
    }

    sz3_interp_decomp_init_runtime(ctx);
    /* 解压时直接引用外部 quant_inds，不做拷贝以降低内存占用。 */
    ctx->quant_inds = quant_inds;
    eb = sz3_line_quantizer_get_eb(&ctx->quantizer);

    if (ctx->anchor_stride == 0) {
        if (quant_count == 0) return NULL;
        /* 无锚点模式下，首元素直接由量化索引恢复。 */
        *dec_data = sz3_line_quantizer_recover(&ctx->quantizer, 0.0f, ctx->quant_inds[ctx->quant_index++]);
    } else {
        sz3_interp_decomp_recover_anchor_grid(ctx, dec_data);
        ctx->interp_level--;
        /* 未完整实现：锚点恢复函数当前仍为占位，复杂锚点场景结果可能不完整。 */
    }

    rctx.ctx = ctx;
    /* quant_count 透传到回调，用于恢复阶段越界保护。 */
    rctx.quant_count = quant_count;
    for (level = ctx->interp_level; level > 0; level--) {
        size_t stride = ((size_t)1) << ((size_t)level - 1u);
        size_t interp_block_size = (size_t)ctx->blocksize * stride;
        size_t block_begin;
        double cur_eb = eb;
        if (ctx->eb_alpha < 0) {
            cur_eb = (level >= 3) ? (eb * ctx->eb_ratio) : eb;
        } else if (ctx->eb_alpha >= 1) {
            double cur_ratio = pow(ctx->eb_alpha, (double)level - 1.0);
            if (cur_ratio > ctx->eb_beta) cur_ratio = ctx->eb_beta;
            cur_eb = eb / cur_ratio;
        }
        sz3_line_quantizer_set_eb(&ctx->quantizer, cur_eb);

        if (ctx->n != 1) {
            /* 未完整实现：当前仅完整支持 1D 解压流程。 */
            continue;
        }
        for (block_begin = 0; block_begin < ctx->original_dimensions[0]; block_begin += interp_block_size) {
            dims_begin[0] = block_begin;
            dims_end[0] = block_begin + interp_block_size;
            if (dims_end[0] > ctx->original_dimensions[0] - 1) {
                dims_end[0] = ctx->original_dimensions[0] - 1;
            }
            sz3_interp_decomp_interpolation(ctx, dec_data, dims_begin, dims_end, ctx->interp_id, sz3_interp_recover_cb,
                                            ctx->direction_sequence_id, stride, &rctx);
        }
    }
    sz3_line_quantizer_set_eb(&ctx->quantizer, eb);
    return dec_data;
}

/* ========== Interpolation Decomposition 到通用 DecompositionOps 的桥接钩子 ========== */
static inline int sz3_interp_decomposition_ops_compress(void *ctx, const SZ3_Config_C *conf, void *data,
                                                        int **quant_inds, size_t *quant_size) {
    SZ3InterpolationDecompositionC *interp_ctx = (SZ3InterpolationDecompositionC *)ctx;
    (void)conf;
    if (interp_ctx == NULL || data == NULL || quant_inds == NULL || quant_size == NULL) return -1;
    *quant_inds = sz3_interp_decomp_compress(interp_ctx, NULL, (float *)data, quant_size);
    return (*quant_inds != NULL || *quant_size == 0) ? 0 : -1;
}

static inline void sz3_interp_decomposition_ops_get_out_range(void *ctx, int *out_begin, int *out_end) {
    SZ3InterpolationDecompositionC *interp_ctx = (SZ3InterpolationDecompositionC *)ctx;
    SZ3RangeI32 r = sz3_interp_decomp_get_out_range(interp_ctx);
    if (out_begin) *out_begin = r.begin;
    if (out_end) *out_end = r.end;
}

static inline size_t sz3_interp_decomposition_ops_size_est(const void *ctx) {
    const SZ3InterpolationDecompositionC *interp_ctx = (const SZ3InterpolationDecompositionC *)ctx;
    if (interp_ctx == NULL || interp_ctx->n == 0 || interp_ctx->n > SZ3_INTERP_MAX_DIMS) return 0;
    return interp_ctx->num_elements * sizeof(int);
}

static inline void sz3_interp_decomposition_ops_save(const void *ctx, sz3_uchar **buffer_pos) {
    sz3_interp_decomp_save((SZ3InterpolationDecompositionC *)ctx, buffer_pos);
}

static inline int sz3_interp_decomposition_ops_load(void *ctx, const sz3_uchar **buffer_pos, size_t *remaining_length) {
    if (ctx == NULL || buffer_pos == NULL || remaining_length == NULL) return -1;
    sz3_interp_decomp_load((SZ3InterpolationDecompositionC *)ctx, buffer_pos, remaining_length);
    return 0;
}

static inline int sz3_interp_decomposition_ops_decompress(void *ctx, const SZ3_Config_C *conf, const int *quant_inds,
                                                          size_t quant_size, void *dec_data) {
    SZ3InterpolationDecompositionC *interp_ctx = (SZ3InterpolationDecompositionC *)ctx;
    (void)conf;
    if (interp_ctx == NULL || quant_inds == NULL || dec_data == NULL) return -1;
    return sz3_interp_decomp_decompress(interp_ctx, NULL, (int *)quant_inds, (float *)dec_data, quant_size) == NULL
               ? -1
               : 0;
}

/* Interpolation decomposition 的通用配置全局实例。 */
static const SZ3_DecompositionOps_C SZ3_InterpolationDecomposition_Ops = {
    sz3_interp_decomposition_ops_compress,   sz3_interp_decomposition_ops_get_out_range,
    sz3_interp_decomposition_ops_size_est,   sz3_interp_decomposition_ops_save,
    sz3_interp_decomposition_ops_load,       sz3_interp_decomposition_ops_decompress};

#ifdef __cplusplus
}
#endif

#endif
