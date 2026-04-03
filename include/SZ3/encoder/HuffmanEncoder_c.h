#ifndef SZ3_HUFFMAN_ENCODER_C_H
#define SZ3_HUFFMAN_ENCODER_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ===================== 非编码模块依赖（空函数占位） =====================
 * 说明：
 * 1) 以下函数是 Huffman 编码流程会依赖的“通用字节流/内存/统计”能力。
 * 2) 目前按需求先给出空函数形式，后续可直接替换为真实实现。
 */

/* ---- 字节序与字节流读写（来自 ByteUtil / MemoryUtil 一类模块） ---- */
static inline void sz3_dep_write_size_t(size_t value, unsigned char **dst) {
    (void)value;
    (void)dst;
}

static inline void sz3_dep_read_size_t(size_t *value, const unsigned char **src) {
    (void)value;
    (void)src;
}

static inline void sz3_dep_read_size_t_with_remaining(size_t *value,
                                                       const unsigned char **src,
                                                       size_t *remaining_length) {
    (void)value;
    (void)src;
    (void)remaining_length;
}

static inline void sz3_dep_int32_to_bytes_big_endian(unsigned char *dst, int32_t value) {
    (void)dst;
    (void)value;
}

static inline int32_t sz3_dep_bytes_to_int32_big_endian(const unsigned char *src) {
    (void)src;
    return 0;
}

static inline void sz3_dep_int64_to_bytes_big_endian(unsigned char *dst, uint64_t value) {
    (void)dst;
    (void)value;
}

/* ---- 统计/哈希频次模块（用于统计 float 出现频次） ---- */
typedef struct SZ3DepFreqEntryF32 {
    float key;
    size_t freq;
} SZ3DepFreqEntryF32;

typedef struct SZ3DepFreqMapF32 {
    SZ3DepFreqEntryF32 *entries;
    size_t size;
    size_t capacity;
} SZ3DepFreqMapF32;

static inline void sz3_dep_freq_map_init(SZ3DepFreqMapF32 *map) {
    (void)map;
}

static inline void sz3_dep_freq_map_inc(SZ3DepFreqMapF32 *map, float key) {
    (void)map;
    (void)key;
}

static inline void sz3_dep_freq_map_destroy(SZ3DepFreqMapF32 *map) {
    (void)map;
}

/*
 * ===================== Huffman C 版本核心数据结构 =====================
 * 说明：
 * - 仅考虑 float 压缩数据类型。
 * - 结构设计对应 HuffmanEncoder.hpp 的类成员与内部结构。
 */

typedef struct SZ3HuffmanNodeF32 {
    struct SZ3HuffmanNodeF32 *left;
    struct SZ3HuffmanNodeF32 *right;
    size_t freq;
    unsigned char is_leaf; /* 1=叶子节点，0=内部节点 */
    float symbol;          /* 叶子节点对应的“状态值索引”（以 float 存储） */
} SZ3HuffmanNodeF32;

typedef struct SZ3HuffmanTreeF32 {
    unsigned int state_num;
    unsigned int all_nodes;

    SZ3HuffmanNodeF32 *pool;
    SZ3HuffmanNodeF32 **qqq;
    SZ3HuffmanNodeF32 **qq; /* 小根堆根节点位于 qq[1] */

    int n_nodes;   /* 编码阶段已使用节点数 */
    int qend;      /* 小根堆尾位置（开区间） */
    int n_inode;   /* 树序列化/反序列化时内部计数 */

    uint64_t **code;     /* 每个状态的 Huffman 码（最多 128 bit，用两个 uint64_t） */
    unsigned char *cout; /* 每个状态的码长（bit） */
    int max_bit_count;
} SZ3HuffmanTreeF32;

typedef struct SZ3HuffmanEncoderF32 {
    SZ3HuffmanTreeF32 *tree;
    SZ3HuffmanNodeF32 *tree_root;

    unsigned int node_count;
    unsigned char sys_endian_type; /* 0=小端, 1=大端 */
    bool loaded;
    float offset; /* 数据最小值（编码偏移基准） */
} SZ3HuffmanEncoderF32;

/*
 * ===================== 生命周期与配置 =====================
 */

static inline unsigned char sz3_huffman_detect_sys_endian_type_f32(void) {
    int x = 1;
    const unsigned char *y = (const unsigned char *)&x;
    return (*y == 1U) ? 0U : 1U;
}

/* 初始化编码器上下文（对标 C++ 构造函数） */
static inline void sz3_huffman_encoder_f32_init(SZ3HuffmanEncoderF32 *enc) {
    if (enc == NULL) return;
    memset(enc, 0, sizeof(*enc));
    enc->sys_endian_type = sz3_huffman_detect_sys_endian_type_f32();
}

/* 释放编码器内部资源（对标 C++ 析构 + SZ_FreeHuffman） */
static inline void sz3_huffman_encoder_f32_destroy(SZ3HuffmanEncoderF32 *enc);

/* 创建 HuffmanTree（对标 createHuffmanTree） */
static inline SZ3HuffmanTreeF32 *sz3_huffman_tree_f32_create(int state_num) {
    if (state_num <= 0) return NULL;

    SZ3HuffmanTreeF32 *tree = (SZ3HuffmanTreeF32 *)malloc(sizeof(SZ3HuffmanTreeF32));
    if (tree == NULL) return NULL;
    memset(tree, 0, sizeof(*tree));

    tree->state_num = (unsigned int)state_num;
    tree->all_nodes = 2U * (unsigned int)state_num;

    size_t node_cap = (size_t)tree->all_nodes * 2U;
    tree->pool = (SZ3HuffmanNodeF32 *)calloc(node_cap, sizeof(SZ3HuffmanNodeF32));
    tree->qqq = (SZ3HuffmanNodeF32 **)calloc(node_cap, sizeof(SZ3HuffmanNodeF32 *));
    tree->code = (uint64_t **)calloc(tree->state_num, sizeof(uint64_t *));
    tree->cout = (unsigned char *)calloc(tree->state_num, sizeof(unsigned char));

    if (tree->pool == NULL || tree->qqq == NULL || tree->code == NULL || tree->cout == NULL) {
        free(tree->pool);
        free(tree->qqq);
        free(tree->code);
        free(tree->cout);
        free(tree);
        return NULL;
    }

    tree->qq = tree->qqq - 1;
    tree->n_nodes = 0;
    tree->n_inode = 0;
    tree->qend = 1;
    tree->max_bit_count = 0;
    return tree;
}

/* 释放 HuffmanTree（C 风格公共接口） */
static inline void sz3_huffman_tree_f32_free(SZ3HuffmanTreeF32 *tree) {
    if (tree == NULL) return;

    free(tree->pool);
    tree->pool = NULL;

    free(tree->qqq);
    tree->qqq = NULL;
    tree->qq = NULL;

    if (tree->code != NULL) {
        for (unsigned int i = 0; i < tree->state_num; i++) {
            free(tree->code[i]);
            tree->code[i] = NULL;
        }
    }
    free(tree->code);
    tree->code = NULL;

    free(tree->cout);
    tree->cout = NULL;

    free(tree);
}

/*
 * ===================== 编码流程函数（对标 EncoderInterface） =====================
 */

/* 预处理：根据输入数据建立 Huffman 树与编码表（对标 preprocess_encode） */
static inline int sz3_huffman_encoder_f32_preprocess_encode(SZ3HuffmanEncoderF32 *enc,
                                                            const float *bins,
                                                            size_t num_bin,
                                                            int state_num_hint);

/* 保存 Huffman 树到输出流（对标 save） */
void sz3_huffman_encoder_f32_save(const SZ3HuffmanEncoderF32 *enc, unsigned char **c);

/* 估算树与元数据大小（对标 size_est） */
size_t sz3_huffman_encoder_f32_size_est(const SZ3HuffmanEncoderF32 *enc);

/* 执行编码（对标 encode） */
size_t sz3_huffman_encoder_f32_encode(const SZ3HuffmanEncoderF32 *enc,
                                      const float *bins,
                                      size_t num_bin,
                                      unsigned char **bytes);

/* 编码后清理（对标 postprocess_encode） */
static inline void sz3_huffman_encoder_f32_postprocess_encode(SZ3HuffmanEncoderF32 *enc);

/* 解码前预处理（对标 preprocess_decode） */
void sz3_huffman_encoder_f32_preprocess_decode(SZ3HuffmanEncoderF32 *enc);

/* 执行解码（对标 decode）
 * 返回值：0 表示成功，非 0 表示失败。
 */
int sz3_huffman_encoder_f32_decode(SZ3HuffmanEncoderF32 *enc,
                                   const unsigned char **bytes,
                                   size_t target_length,
                                   float *out_values);

/* 解码后清理（对标 postprocess_decode） */
void sz3_huffman_encoder_f32_postprocess_decode(SZ3HuffmanEncoderF32 *enc);

/* 从输入流加载 Huffman 树（对标 load） */
int sz3_huffman_encoder_f32_load(SZ3HuffmanEncoderF32 *enc,
                                 const unsigned char **c,
                                 size_t *remaining_length);

/* 查询是否已成功 load（对标 isLoaded） */
bool sz3_huffman_encoder_f32_is_loaded(const SZ3HuffmanEncoderF32 *enc);

/*
 * ===================== 内部算法函数（按函数级从 C++ 私有函数平移） =====================
 * 说明：
 * - 这些函数建议在 .c 文件中定义为 static；
 * - 头文件里先统一声明，方便后续逐个实现。
 */

SZ3HuffmanNodeF32 *sz3_huffman_reconstruct_tree_from_bytes_any_states_f32(SZ3HuffmanEncoderF32 *enc,
                                                                           const unsigned char *bytes,
                                                                           unsigned int node_count);

static inline SZ3HuffmanNodeF32 *sz3_huffman_new_node_f32(SZ3HuffmanEncoderF32 *enc,
                                                          size_t freq,
                                                          float symbol,
                                                          SZ3HuffmanNodeF32 *left,
                                                          SZ3HuffmanNodeF32 *right);

SZ3HuffmanNodeF32 *sz3_huffman_new_node2_f32(SZ3HuffmanEncoderF32 *enc,
                                             float symbol,
                                             unsigned char is_leaf);

static inline void sz3_huffman_qinsert_f32(SZ3HuffmanEncoderF32 *enc, SZ3HuffmanNodeF32 *node);
static inline SZ3HuffmanNodeF32 *sz3_huffman_qremove_f32(SZ3HuffmanEncoderF32 *enc);

static inline void sz3_huffman_build_code_f32(SZ3HuffmanEncoderF32 *enc,
                                              SZ3HuffmanNodeF32 *node,
                                              int len,
                                              uint64_t out1,
                                              uint64_t out2);

static inline int sz3_huffman_init_f32(SZ3HuffmanEncoderF32 *enc, const float *input, size_t length);

void sz3_huffman_pad_tree_u8_f32(SZ3HuffmanEncoderF32 *enc,
                                 uint8_t *L,
                                 uint8_t *R,
                                 float *C,
                                 unsigned char *t,
                                 unsigned int i,
                                 SZ3HuffmanNodeF32 *root);

void sz3_huffman_pad_tree_u16_f32(SZ3HuffmanEncoderF32 *enc,
                                  uint16_t *L,
                                  uint16_t *R,
                                  float *C,
                                  unsigned char *t,
                                  unsigned int i,
                                  SZ3HuffmanNodeF32 *root);

void sz3_huffman_pad_tree_u32_f32(SZ3HuffmanEncoderF32 *enc,
                                  uint32_t *L,
                                  uint32_t *R,
                                  float *C,
                                  unsigned char *t,
                                  unsigned int i,
                                  SZ3HuffmanNodeF32 *root);

void sz3_huffman_unpad_tree_u8_f32(SZ3HuffmanEncoderF32 *enc,
                                   const uint8_t *L,
                                   const uint8_t *R,
                                   const float *C,
                                   const unsigned char *t,
                                   unsigned int i,
                                   SZ3HuffmanNodeF32 *root);

void sz3_huffman_unpad_tree_u16_f32(SZ3HuffmanEncoderF32 *enc,
                                    const uint16_t *L,
                                    const uint16_t *R,
                                    const float *C,
                                    const unsigned char *t,
                                    unsigned int i,
                                    SZ3HuffmanNodeF32 *root);

void sz3_huffman_unpad_tree_u32_f32(SZ3HuffmanEncoderF32 *enc,
                                    const uint32_t *L,
                                    const uint32_t *R,
                                    const float *C,
                                    const unsigned char *t,
                                    unsigned int i,
                                    SZ3HuffmanNodeF32 *root);

unsigned int sz3_huffman_convert_tree_to_bytes_u8_f32(SZ3HuffmanEncoderF32 *enc,
                                                       unsigned int node_count,
                                                       unsigned char *out);

unsigned int sz3_huffman_convert_tree_to_bytes_u16_f32(SZ3HuffmanEncoderF32 *enc,
                                                        unsigned int node_count,
                                                        unsigned char *out);

unsigned int sz3_huffman_convert_tree_to_bytes_u32_f32(SZ3HuffmanEncoderF32 *enc,
                                                        unsigned int node_count,
                                                        unsigned char *out);

/* 释放编码器内部 Huffman 资源（对标 SZ_FreeHuffman） */
static inline void sz3_huffman_free_internal_f32(SZ3HuffmanEncoderF32 *enc);

static inline SZ3HuffmanNodeF32 *sz3_huffman_new_node_f32(SZ3HuffmanEncoderF32 *enc,
                                                          size_t freq,
                                                          float symbol,
                                                          SZ3HuffmanNodeF32 *left,
                                                          SZ3HuffmanNodeF32 *right) {
    if (enc == NULL || enc->tree == NULL || enc->tree->pool == NULL) return NULL;
    SZ3HuffmanTreeF32 *tree = enc->tree;
    SZ3HuffmanNodeF32 *n = tree->pool + tree->n_nodes++;
    if (freq) {
        n->symbol = symbol;
        n->freq = freq;
        n->is_leaf = 1U;
        n->left = NULL;
        n->right = NULL;
    } else {
        n->left = left;
        n->right = right;
        n->freq = (left ? left->freq : 0U) + (right ? right->freq : 0U);
        n->is_leaf = 0U;
        n->symbol = 0.0f;
    }
    return n;
}

static inline void sz3_huffman_qinsert_f32(SZ3HuffmanEncoderF32 *enc, SZ3HuffmanNodeF32 *node) {
    if (enc == NULL || enc->tree == NULL || node == NULL) return;
    SZ3HuffmanTreeF32 *tree = enc->tree;
    int j;
    int i = tree->qend++;
    /* 维护最小堆性质，按频次上浮插入节点。 */
    while ((j = (i >> 1)) != 0) {
        if (tree->qq[j]->freq <= node->freq) break;
        tree->qq[i] = tree->qq[j];
        i = j;
    }
    tree->qq[i] = node;
}

static inline SZ3HuffmanNodeF32 *sz3_huffman_qremove_f32(SZ3HuffmanEncoderF32 *enc) {
    if (enc == NULL || enc->tree == NULL) return NULL;
    SZ3HuffmanTreeF32 *tree = enc->tree;
    if (tree->qend < 2) return NULL;

    int i = 1;
    /* 弹出堆顶（当前最小频次节点）。 */
    SZ3HuffmanNodeF32 *n = tree->qq[1];
    tree->qend--;
    tree->qq[i] = tree->qq[tree->qend];

    int l;
    /* 下沉恢复最小堆结构。 */
    while ((l = (i << 1)) < tree->qend) {
        if (l + 1 < tree->qend && tree->qq[l + 1]->freq < tree->qq[l]->freq) l++;
        if (tree->qq[i]->freq > tree->qq[l]->freq) {
            SZ3HuffmanNodeF32 *p = tree->qq[i];
            tree->qq[i] = tree->qq[l];
            tree->qq[l] = p;
            i = l;
        } else {
            break;
        }
    }
    return n;
}

static inline void sz3_huffman_build_code_f32(SZ3HuffmanEncoderF32 *enc,
                                              SZ3HuffmanNodeF32 *node,
                                              int len,
                                              uint64_t out1,
                                              uint64_t out2) {
    if (enc == NULL || enc->tree == NULL || node == NULL) return;
    SZ3HuffmanTreeF32 *tree = enc->tree;
    if (node->is_leaf) {
        int symbol = (int)node->symbol;
        if (symbol < 0 || (unsigned int)symbol >= tree->state_num) return;
        tree->code[symbol] = (uint64_t *)malloc(2U * sizeof(uint64_t));
        if (tree->code[symbol] == NULL) return;
        /* 叶子节点落盘为最终码字与码长。 */
        if (len == 0) {
            tree->code[symbol][0] = 0;
            tree->code[symbol][1] = 0;
        } else if (len <= 64) {
            tree->code[symbol][0] = out1 << (64 - len);
            tree->code[symbol][1] = out2;
        } else {
            tree->code[symbol][0] = out1;
            tree->code[symbol][1] = out2 << (128 - len);
        }
        tree->cout[symbol] = (unsigned char)len;
        if (len > tree->max_bit_count) tree->max_bit_count = len;
        return;
    }

    int index = len >> 6;
    if (index == 0) {
        /* 向左分支追加 bit=0。 */
        out1 = out1 << 1;
        sz3_huffman_build_code_f32(enc, node->left, len + 1, out1, 0);
        /* 向右分支追加 bit=1。 */
        out1 = out1 | 1U;
        sz3_huffman_build_code_f32(enc, node->right, len + 1, out1, 0);
    } else {
        if (len % 64 != 0) out2 = out2 << 1;
        sz3_huffman_build_code_f32(enc, node->left, len + 1, out1, out2);
        out2 = out2 | 1U;
        sz3_huffman_build_code_f32(enc, node->right, len + 1, out1, out2);
    }
}

static inline int sz3_huffman_init_f32(SZ3HuffmanEncoderF32 *enc, const float *input, size_t length) {
    if (enc == NULL || input == NULL || length == 0) return -1;

    /* 扫描输入，确定偏移量 offset(最小值) 与最大值。 */
    float max_v = input[0];
    enc->offset = input[0];
    for (size_t i = 1; i < length; i++) {
        if (input[i] > max_v) max_v = input[i];
        if (input[i] < enc->offset) enc->offset = input[i];
    }

    int state_num = (int)(max_v - enc->offset + 2.0f);
    if (state_num <= 0) return -1;

    /* 按状态数创建 HuffmanTree 的节点池、堆和编码表。 */
    enc->tree = sz3_huffman_tree_f32_create(state_num);
    if (enc->tree == NULL) return -1;

    size_t *frequency_list = (size_t *)calloc((size_t)state_num, sizeof(size_t));
    if (frequency_list == NULL) {
        sz3_huffman_tree_f32_free(enc->tree);
        enc->tree = NULL;
        return -1;
    }

    for (size_t i = 0; i < length; i++) {
        float shifted = input[i] - enc->offset;
        int idx = (int)shifted;
        /* 仅接受“可映射到整数状态索引”的输入。 */
        if (idx < 0 || idx >= state_num || fabsf(shifted - (float)idx) > 1e-6f) {
            free(frequency_list);
            sz3_huffman_tree_f32_free(enc->tree);
            enc->tree = NULL;
            return -1;
        }
        frequency_list[idx] += 1U;
    }

    /* 将非零频次状态作为叶子节点插入最小堆。 */
    for (int i = 0; i < state_num; i++) {
        if (frequency_list[i] != 0) {
            sz3_huffman_qinsert_f32(enc, sz3_huffman_new_node_f32(enc, frequency_list[i], (float)i, NULL, NULL));
        }
    }

    free(frequency_list);

    /* 反复取出两个最小频次节点并合并，直到得到根节点。 */
    while (enc->tree->qend > 2) {
        SZ3HuffmanNodeF32 *left = sz3_huffman_qremove_f32(enc);
        SZ3HuffmanNodeF32 *right = sz3_huffman_qremove_f32(enc);
        sz3_huffman_qinsert_f32(enc, sz3_huffman_new_node_f32(enc, 0, 0, left, right));
    }

    /* 从根递归生成每个状态的 Huffman 码。 */
    enc->tree_root = enc->tree->qq[1];
    sz3_huffman_build_code_f32(enc, enc->tree_root, 0, 0, 0);
    return 0;
}

static inline int sz3_huffman_encoder_f32_preprocess_encode(SZ3HuffmanEncoderF32 *enc,
                                                            const float *bins,
                                                            size_t num_bin,
                                                            int state_num_hint) {
    (void)state_num_hint;
    if (enc == NULL || bins == NULL || num_bin == 0) return -1;

    /* 预处理前先释放历史树，避免跨批次污染。 */
    sz3_huffman_free_internal_f32(enc);
    enc->node_count = 0;
    if (sz3_huffman_init_f32(enc, bins, num_bin) != 0 || enc->tree == NULL) return -1;

    /* 统计有效叶子数，并换算序列化树节点总数。 */
    for (unsigned int i = 0; i < enc->tree->state_num; i++) {
        if (enc->tree->code[i] != NULL) enc->node_count++;
    }
    enc->node_count = enc->node_count * 2U - 1U;
    return 0;
}

static inline void sz3_huffman_encoder_f32_postprocess_encode(SZ3HuffmanEncoderF32 *enc) {
    /* 编码完成后释放 Huffman 内部内存。 */
    sz3_huffman_free_internal_f32(enc);
}

static inline void sz3_huffman_free_internal_f32(SZ3HuffmanEncoderF32 *enc) {
    if (enc == NULL) return;
    if (enc->tree != NULL) {
        /* 统一回收树池/堆/码表，重置指针状态。 */
        sz3_huffman_tree_f32_free(enc->tree);
        enc->tree = NULL;
    }
    enc->tree_root = NULL;
    enc->node_count = 0;
}

static inline void sz3_huffman_encoder_f32_destroy(SZ3HuffmanEncoderF32 *enc) {
    if (enc == NULL) return;
    sz3_huffman_free_internal_f32(enc);
    enc->loaded = false;
}

#ifdef __cplusplus
}
#endif

#endif /* SZ3_HUFFMAN_ENCODER_C_H */
