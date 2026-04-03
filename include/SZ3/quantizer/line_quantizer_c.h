#ifndef SZ3_LINE_QUANTIZER_C_H
#define SZ3_LINE_QUANTIZER_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ===================== 依赖的非线性量化模块（占位空函数） =====================
 * 说明：
 * 1) 这些函数用于声明 Line Quantizer 未来可能依赖的非线性量化能力。
 * 2) 当前按需求先给出“空函数形式”，后续接入真实实现时可直接替换。
 */
static inline void sz3_nonlinear_quantizer_prepare(void) {}

static inline int sz3_nonlinear_quantizer_quantize(float data, float pred, float *dec_data) {
    (void)data;
    (void)pred;
    (void)dec_data;
    return 0;
}

static inline float sz3_nonlinear_quantizer_recover(float pred, int quant_index) {
    (void)pred;
    (void)quant_index;
    return 0.0f;
}

static inline void sz3_nonlinear_quantizer_release(void) {}

/*
 * ===================== Line Quantizer C 版本数据结构 =====================
 */

/* 输出范围结构体：对应 C++ 中 get_out_range() 的返回值 pair<int, int> */
typedef struct SZ3RangeI32 {
    int min;
    int max;
} SZ3RangeI32;

/*
 * Line Quantizer 主结构体（对标 LinearQuantizer<T>）
 * 说明：
 * - 当前仅考虑 float 类型压缩数据。
 */
typedef struct SZ3LineQuantizerC {
    /* 量化误差控制参数 */
    float error_bound;
    float error_bound_reciprocal;

    /* 量化半径（对应 C++ radius） */
    int radius;

    /* 是否严格误差界（对应 strict_eb） */
    bool strict_eb;

    /* 反序列化标识（对应 C++ uid = 0b10） */
    uint8_t uid;

    /* 不可预测值缓冲区（对应 std::vector<T> unpred） */
    float *unpred;
    size_t unpred_size;
    size_t unpred_capacity;

    /* 解压时读取 unpred 的游标（对应 C++ index） */
    size_t index;
} SZ3LineQuantizerC;

/*
 * ===================== 内存与生命周期管理函数 =====================
 */

/* 默认初始化：error_bound=1, radius=32768, strict_eb=true */
static inline void sz3_line_quantizer_init_default(SZ3LineQuantizerC *q);

/* 参数初始化：对标 C++ 构造函数 LinearQuantizer(float eb, int r, bool strict) */
static inline void sz3_line_quantizer_init(SZ3LineQuantizerC *q, float eb, int r, bool strict_eb);

/* 释放内部资源（主要是 unpred 缓冲区） */
void sz3_line_quantizer_destroy(SZ3LineQuantizerC *q);

/* 清空运行态数据但保留配置参数 */
void sz3_line_quantizer_reset_runtime(SZ3LineQuantizerC *q);

/*
 * ===================== 参数访问函数 =====================
 */

/* 获取误差界（对标 get_eb） */
static inline float sz3_line_quantizer_get_eb(const SZ3LineQuantizerC *q);

/* 设置误差界并刷新倒数（对标 set_eb） */
static inline void sz3_line_quantizer_set_eb(SZ3LineQuantizerC *q, float eb);

/* 获取输出索引范围（对标 get_out_range，范围为 [0, radius * 2]） */
static inline SZ3RangeI32 sz3_line_quantizer_get_out_range(const SZ3LineQuantizerC *q);

/*
 * ===================== 量化/反量化核心函数 =====================
 */

/*
 * 量化并覆写输入值（对标 quantize_and_overwrite）
 * - 入参 data 为“原始值”，函数内会在可预测时覆写为“解压重建值”；
 * - 返回量化索引，返回 0 表示不可预测并写入 unpred。
 */
static inline int sz3_line_quantizer_quantize_and_overwrite(SZ3LineQuantizerC *q, float *data, float pred);

/*
 * 根据量化索引恢复数据（对标 recover）
 * - quant_index != 0：走预测恢复；
 * - quant_index == 0：从 unpred 按顺序读取。
 */
float sz3_line_quantizer_recover(SZ3LineQuantizerC *q, float pred, int quant_index);

/* 预测值路径恢复（对标 recover_pred） */
float sz3_line_quantizer_recover_pred(const SZ3LineQuantizerC *q, float pred, int quant_index);

/* 不可预测值路径恢复（对标 recover_unpred） */
float sz3_line_quantizer_recover_unpred(SZ3LineQuantizerC *q);

/* 强制将原值作为不可预测值保存（对标 force_save_unpred） */
static inline int sz3_line_quantizer_force_save_unpred(SZ3LineQuantizerC *q, float ori);

/*
 * ===================== 序列化与统计函数 =====================
 */

/* 估算 unpred 存储大小（字节数，对标 size_est） */
size_t sz3_line_quantizer_size_est(const SZ3LineQuantizerC *q);

/*
 * 保存到字节流（对标 save）
 * - c 为可写指针，写入后按已写入长度向后移动。
 */
void sz3_line_quantizer_save(const SZ3LineQuantizerC *q, unsigned char **c);

/*
 * 从字节流加载（对标 load）
 * - c 为只读指针，读取后按已读取长度向后移动；
 * - remaining_length 会按读取量递减。
 */
void sz3_line_quantizer_load(SZ3LineQuantizerC *q, const unsigned char **c, size_t *remaining_length);

/* 打印状态信息（对标 print） */
void sz3_line_quantizer_print(const SZ3LineQuantizerC *q, FILE *out);

/*
 * ===================== 内部辅助函数（建议实现为 static） =====================
 */

/* unpred 扩容，至少保证可容纳 min_capacity 个元素 */
static inline int sz3_line_quantizer_reserve_unpred(SZ3LineQuantizerC *q, size_t min_capacity);

/* 向 unpred 末尾追加一个值 */
static inline int sz3_line_quantizer_push_unpred(SZ3LineQuantizerC *q, float value);

/* ===================== 内联实现 ===================== */

static inline int sz3_line_quantizer_reserve_unpred(SZ3LineQuantizerC *q, size_t min_capacity) {
    size_t new_capacity;
    float *new_buf;

    if (q == NULL) {
        return -1;
    }
    if (q->unpred_capacity >= min_capacity) {
        return 0;
    }

    new_capacity = (q->unpred_capacity == 0) ? 64u : q->unpred_capacity;
    while (new_capacity < min_capacity) {
        size_t doubled = new_capacity * 2u;
        if (doubled < new_capacity) {
            new_capacity = min_capacity;
            break;
        }
        new_capacity = doubled;
    }

    new_buf = (float *)realloc(q->unpred, new_capacity * sizeof(float));
    if (new_buf == NULL) {
        return -1;
    }

    q->unpred = new_buf;
    q->unpred_capacity = new_capacity;
    return 0;
}

static inline int sz3_line_quantizer_push_unpred(SZ3LineQuantizerC *q, float value) {
    if (q == NULL) {
        return -1;
    }
    if (sz3_line_quantizer_reserve_unpred(q, q->unpred_size + 1) != 0) {
        return -1;
    }
    q->unpred[q->unpred_size++] = value;
    return 0;
}

static inline int sz3_line_quantizer_force_save_unpred(SZ3LineQuantizerC *q, float ori) {
    return sz3_line_quantizer_push_unpred(q, ori) == 0 ? 0 : -1;
}

static inline void sz3_line_quantizer_init(SZ3LineQuantizerC *q, float eb, int r, bool strict_eb) {
    if (q == NULL) {
        return;
    }
    if (eb == 0.0f) {
        eb = 1.0f;
    }

    q->error_bound = eb;
    q->error_bound_reciprocal = 1.0f / eb;
    q->radius = r;
    q->strict_eb = strict_eb;
    q->uid = 0b10;

    q->unpred = NULL;
    q->unpred_size = 0;
    q->unpred_capacity = 0;
    q->index = 0;
}

static inline void sz3_line_quantizer_init_default(SZ3LineQuantizerC *q) {
    sz3_line_quantizer_init(q, 1.0f, 32768, true);
}

static inline float sz3_line_quantizer_get_eb(const SZ3LineQuantizerC *q) {
    return (q == NULL) ? 0.0f : q->error_bound;
}

static inline void sz3_line_quantizer_set_eb(SZ3LineQuantizerC *q, float eb) {
    if (q == NULL) {
        return;
    }
    if (eb == 0.0f) {
        eb = 1.0f;
    }
    q->error_bound = eb;
    q->error_bound_reciprocal = 1.0f / eb;
}

static inline SZ3RangeI32 sz3_line_quantizer_get_out_range(const SZ3LineQuantizerC *q) {
    SZ3RangeI32 range;
    range.min = 0;
    range.max = (q == NULL) ? 0 : q->radius * 2;
    return range;
}

static inline int sz3_line_quantizer_quantize_and_overwrite(SZ3LineQuantizerC *q, float *data, float pred) {
    float diff;
    int64_t quant_index;
    int half_index;
    int quant_index_shifted;
    float decompressed_data;

    if (q == NULL || data == NULL) {
        return 0;
    }

    diff = *data - pred;
    quant_index = (int64_t)(fabsf(diff) * q->error_bound_reciprocal) + 1;
    if (quant_index < (int64_t)q->radius * 2) {
        quant_index >>= 1;
        half_index = (int)quant_index;
        quant_index <<= 1;

        if (diff < 0) {
            quant_index = -quant_index;
            quant_index_shifted = q->radius - half_index;
        } else {
            quant_index_shifted = q->radius + half_index;
        }

        decompressed_data = pred + (float)quant_index * q->error_bound;
        diff = fabsf(decompressed_data - *data);

        if (diff <= q->error_bound || (!q->strict_eb && diff <= q->error_bound * 1.1f)) {
            *data = decompressed_data;
            return quant_index_shifted;
        }
    }

    (void)sz3_line_quantizer_force_save_unpred(q, *data);
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* SZ3_LINE_QUANTIZER_C_H */
