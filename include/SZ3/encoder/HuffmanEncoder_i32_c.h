#ifndef SZ3_HUFFMAN_ENCODER_I32_C_H
#define SZ3_HUFFMAN_ENCODER_I32_C_H

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
static inline void sz3_dep_i32_write_size_t(size_t value, unsigned char **dst) {
    if (dst == NULL || *dst == NULL) return;
    memcpy(*dst, &value, sizeof(size_t));
    *dst += sizeof(size_t);
}

static inline void sz3_dep_i32_read_size_t(size_t *value, const unsigned char **src) {
    if (value == NULL || src == NULL || *src == NULL) return;
    memcpy(value, *src, sizeof(size_t));
    *src += sizeof(size_t);
}

static inline void sz3_dep_i32_read_size_t_with_remaining(size_t *value,
                                                       const unsigned char **src,
                                                       size_t *remaining_length) {
    if (value == NULL || src == NULL || *src == NULL || remaining_length == NULL) return;
    if (*remaining_length < sizeof(size_t)) {
        *value = 0;
        return;
    }
    memcpy(value, *src, sizeof(size_t));
    *src += sizeof(size_t);
    *remaining_length -= sizeof(size_t);
}

static inline void sz3_dep_i32_int32_to_bytes_big_endian(unsigned char *dst, int32_t value) {
    if (dst == NULL) return;
    dst[0] = (unsigned char)((uint32_t)value >> 24);
    dst[1] = (unsigned char)((uint32_t)value >> 16);
    dst[2] = (unsigned char)((uint32_t)value >> 8);
    dst[3] = (unsigned char)((uint32_t)value);
}

static inline int32_t sz3_dep_i32_bytes_to_int32_big_endian(const unsigned char *src) {
    if (src == NULL) return 0;
    int32_t res = 0;
    res |= (int32_t)src[0];
    res <<= 8;
    res |= (int32_t)src[1];
    res <<= 8;
    res |= (int32_t)src[2];
    res <<= 8;
    res |= (int32_t)src[3];
    return res;
}

static inline void sz3_dep_i32_int64_to_bytes_big_endian(unsigned char *dst, uint64_t value) {
    if (dst == NULL) return;
    dst[0] = (unsigned char)(value >> 56);
    dst[1] = (unsigned char)(value >> 48);
    dst[2] = (unsigned char)(value >> 40);
    dst[3] = (unsigned char)(value >> 32);
    dst[4] = (unsigned char)(value >> 24);
    dst[5] = (unsigned char)(value >> 16);
    dst[6] = (unsigned char)(value >> 8);
    dst[7] = (unsigned char)(value);
}

/* ---- 统计/哈希频次模块（用于统计 int 出现频次） ---- */
typedef struct SZ3DepFreqEntryI32 {
    int key;
    size_t freq;
} SZ3DepFreqEntryI32;

typedef struct SZ3DepFreqMapI32 {
    SZ3DepFreqEntryI32 *entries;
    size_t size;
    size_t capacity;
} SZ3DepFreqMapI32;

static inline void sz3_dep_i32_freq_map_init(SZ3DepFreqMapI32 *map) {
    (void)map;
}

static inline void sz3_dep_i32_freq_map_inc(SZ3DepFreqMapI32 *map, int key) {
    (void)map;
    (void)key;
}

static inline void sz3_dep_i32_freq_map_destroy(SZ3DepFreqMapI32 *map) {
    (void)map;
}

/*
 * ===================== Huffman C 版本核心数据结构 =====================
 * 说明：
 * - 仅考虑 int 压缩数据类型。
 * - 结构设计对应 HuffmanEncoder.hpp 的类成员与内部结构。
 */

typedef struct SZ3HuffmanNodeI32 {
    struct SZ3HuffmanNodeI32 *left;
    struct SZ3HuffmanNodeI32 *right;
    size_t freq;
    unsigned char is_leaf; /* 1=叶子节点，0=内部节点 */
    int symbol;          /* 叶子节点对应的“状态值索引”（以 int 存储） */
} SZ3HuffmanNodeI32;

typedef struct SZ3HuffmanTreeI32 {
    unsigned int state_num;
    unsigned int all_nodes;

    SZ3HuffmanNodeI32 *pool;
    SZ3HuffmanNodeI32 **qqq;
    SZ3HuffmanNodeI32 **qq; /* 小根堆根节点位于 qq[1] */

    int n_nodes;   /* 编码阶段已使用节点数 */
    int qend;      /* 小根堆尾位置（开区间） */
    int n_inode;   /* 树序列化/反序列化时内部计数 */

    uint64_t **code;     /* 每个状态的 Huffman 码（最多 128 bit，用两个 uint64_t） */
    unsigned char *cout; /* 每个状态的码长（bit） */
    int max_bit_count;
} SZ3HuffmanTreeI32;

typedef struct SZ3HuffmanEncoderI32 {
    SZ3HuffmanTreeI32 *tree;
    SZ3HuffmanNodeI32 *tree_root;

    unsigned int node_count;
    unsigned char sys_endian_type; /* 0=小端, 1=大端 */
    bool loaded;
    int offset; /* 数据最小值（编码偏移基准） */
} SZ3HuffmanEncoderI32;

/*
 * ===================== 生命周期与配置 =====================
 */

static inline unsigned char sz3_huffman_detect_sys_endian_type_i32(void) {
    int x = 1;
    const unsigned char *y = (const unsigned char *)&x;
    return (*y == 1U) ? 0U : 1U;
}

/* 初始化编码器上下文（对标 C++ 构造函数） */
static inline void sz3_huffman_encoder_i32_init(SZ3HuffmanEncoderI32 *enc) {
    if (enc == NULL) return;
    memset(enc, 0, sizeof(*enc));
    enc->sys_endian_type = sz3_huffman_detect_sys_endian_type_i32();
}

/* 释放编码器内部资源（对标 C++ 析构 + SZ_FreeHuffman） */
static inline void sz3_huffman_encoder_i32_destroy(SZ3HuffmanEncoderI32 *enc);

/* 创建 HuffmanTree（对标 createHuffmanTree） */
static inline SZ3HuffmanTreeI32 *sz3_huffman_tree_i32_create(int state_num) {
    if (state_num <= 0) return NULL;

    SZ3HuffmanTreeI32 *tree = (SZ3HuffmanTreeI32 *)malloc(sizeof(SZ3HuffmanTreeI32));
    if (tree == NULL) return NULL;
    memset(tree, 0, sizeof(*tree));

    tree->state_num = (unsigned int)state_num;
    tree->all_nodes = 2U * (unsigned int)state_num;

    size_t node_cap = (size_t)tree->all_nodes * 2U;
    tree->pool = (SZ3HuffmanNodeI32 *)calloc(node_cap, sizeof(SZ3HuffmanNodeI32));
    tree->qqq = (SZ3HuffmanNodeI32 **)calloc(node_cap, sizeof(SZ3HuffmanNodeI32 *));
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
static inline void sz3_huffman_tree_i32_free(SZ3HuffmanTreeI32 *tree) {
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
static inline int sz3_huffman_encoder_i32_preprocess_encode(SZ3HuffmanEncoderI32 *enc,
                                                            const int *bins,
                                                            size_t num_bin,
                                                            int state_num_hint);

/* 保存 Huffman 树到输出流（对标 save） */
static inline void sz3_huffman_encoder_i32_save(const SZ3HuffmanEncoderI32 *enc, unsigned char **c);

/* 估算树与元数据大小（对标 size_est） */
static inline size_t sz3_huffman_encoder_i32_size_est(const SZ3HuffmanEncoderI32 *enc);

/* 执行编码（对标 encode） */
static inline size_t sz3_huffman_encoder_i32_encode(const SZ3HuffmanEncoderI32 *enc,
                                                    const int *bins,
                                                    size_t num_bin,
                                                    unsigned char **bytes);

/* 编码后清理（对标 postprocess_encode） */
static inline void sz3_huffman_encoder_i32_postprocess_encode(SZ3HuffmanEncoderI32 *enc);

/* 解码前预处理（对标 preprocess_decode） */
static inline void sz3_huffman_encoder_i32_preprocess_decode(SZ3HuffmanEncoderI32 *enc);

/* 执行解码（对标 decode）
 * 返回值：0 表示成功，非 0 表示失败。
 */
static inline int sz3_huffman_encoder_i32_decode(SZ3HuffmanEncoderI32 *enc,
                                                 const unsigned char **bytes,
                                                 size_t target_length,
                                                 int *out_values);

/* 解码后清理（对标 postprocess_decode） */
static inline void sz3_huffman_encoder_i32_postprocess_decode(SZ3HuffmanEncoderI32 *enc);

/* 从输入流加载 Huffman 树（对标 load） */
static inline int sz3_huffman_encoder_i32_load(SZ3HuffmanEncoderI32 *enc,
                                                const unsigned char **c,
                                                size_t *remaining_length);

/* 查询是否已成功 load（对标 isLoaded） */
static inline bool sz3_huffman_encoder_i32_is_loaded(const SZ3HuffmanEncoderI32 *enc);

/*
 * ===================== 内部算法函数（按函数级从 C++ 私有函数平移） =====================
 * 说明：
 * - 这些函数建议在 .c 文件中定义为 static；
 * - 头文件里先统一声明，方便后续逐个实现。
 */

static inline SZ3HuffmanNodeI32 *sz3_huffman_reconstruct_tree_from_bytes_any_states_i32(SZ3HuffmanEncoderI32 *enc,
                                                                                          const unsigned char *bytes,
                                                                                          unsigned int node_count);

static inline SZ3HuffmanNodeI32 *sz3_huffman_new_node_i32(SZ3HuffmanEncoderI32 *enc,
                                                          size_t freq,
                                                          int symbol,
                                                          SZ3HuffmanNodeI32 *left,
                                                          SZ3HuffmanNodeI32 *right);

static inline SZ3HuffmanNodeI32 *sz3_huffman_new_node2_i32(SZ3HuffmanEncoderI32 *enc,
                                                            int symbol,
                                                            unsigned char is_leaf);

static inline void sz3_huffman_qinsert_i32(SZ3HuffmanEncoderI32 *enc, SZ3HuffmanNodeI32 *node);
static inline SZ3HuffmanNodeI32 *sz3_huffman_qremove_i32(SZ3HuffmanEncoderI32 *enc);

static inline void sz3_huffman_build_code_i32(SZ3HuffmanEncoderI32 *enc,
                                              SZ3HuffmanNodeI32 *node,
                                              int len,
                                              uint64_t out1,
                                              uint64_t out2);

static inline int sz3_huffman_init_i32(SZ3HuffmanEncoderI32 *enc, const int *input, size_t length);

static inline void sz3_huffman_pad_tree_u8_i32(SZ3HuffmanEncoderI32 *enc,
                                                uint8_t *L,
                                                uint8_t *R,
                                                int *C,
                                                unsigned char *t,
                                                unsigned int i,
                                                SZ3HuffmanNodeI32 *root);

static inline void sz3_huffman_pad_tree_u16_i32(SZ3HuffmanEncoderI32 *enc,
                                                 uint16_t *L,
                                                 uint16_t *R,
                                                 int *C,
                                                 unsigned char *t,
                                                 unsigned int i,
                                                 SZ3HuffmanNodeI32 *root);

static inline void sz3_huffman_pad_tree_u32_i32(SZ3HuffmanEncoderI32 *enc,
                                                 uint32_t *L,
                                                 uint32_t *R,
                                                 int *C,
                                                 unsigned char *t,
                                                 unsigned int i,
                                                 SZ3HuffmanNodeI32 *root);

static inline void sz3_huffman_unpad_tree_u8_i32(SZ3HuffmanEncoderI32 *enc,
                                                  const uint8_t *L,
                                                  const uint8_t *R,
                                                  const int *C,
                                                  const unsigned char *t,
                                                  unsigned int i,
                                                  SZ3HuffmanNodeI32 *root);

static inline void sz3_huffman_unpad_tree_u16_i32(SZ3HuffmanEncoderI32 *enc,
                                                   const uint16_t *L,
                                                   const uint16_t *R,
                                                   const int *C,
                                                   const unsigned char *t,
                                                   unsigned int i,
                                                   SZ3HuffmanNodeI32 *root);

static inline void sz3_huffman_unpad_tree_u32_i32(SZ3HuffmanEncoderI32 *enc,
                                                   const uint32_t *L,
                                                   const uint32_t *R,
                                                   const int *C,
                                                   const unsigned char *t,
                                                   unsigned int i,
                                                   SZ3HuffmanNodeI32 *root);

static inline unsigned int sz3_huffman_convert_tree_to_bytes_u8_i32(SZ3HuffmanEncoderI32 *enc,
                                                                     unsigned int node_count,
                                                                     unsigned char *out);

static inline unsigned int sz3_huffman_convert_tree_to_bytes_u16_i32(SZ3HuffmanEncoderI32 *enc,
                                                                      unsigned int node_count,
                                                                      unsigned char *out);

static inline unsigned int sz3_huffman_convert_tree_to_bytes_u32_i32(SZ3HuffmanEncoderI32 *enc,
                                                                      unsigned int node_count,
                                                                      unsigned char *out);

/* 释放编码器内部 Huffman 资源（对标 SZ_FreeHuffman） */
static inline void sz3_huffman_free_internal_i32(SZ3HuffmanEncoderI32 *enc);

static inline SZ3HuffmanNodeI32 *sz3_huffman_new_node_i32(SZ3HuffmanEncoderI32 *enc,
                                                          size_t freq,
                                                          int symbol,
                                                          SZ3HuffmanNodeI32 *left,
                                                          SZ3HuffmanNodeI32 *right) {
    if (enc == NULL || enc->tree == NULL || enc->tree->pool == NULL) return NULL;
    SZ3HuffmanTreeI32 *tree = enc->tree;
    SZ3HuffmanNodeI32 *n = tree->pool + tree->n_nodes++;
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
        n->symbol = 0;
    }
    return n;
}

static inline void sz3_huffman_qinsert_i32(SZ3HuffmanEncoderI32 *enc, SZ3HuffmanNodeI32 *node) {
    if (enc == NULL || enc->tree == NULL || node == NULL) return;
    SZ3HuffmanTreeI32 *tree = enc->tree;
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

static inline SZ3HuffmanNodeI32 *sz3_huffman_qremove_i32(SZ3HuffmanEncoderI32 *enc) {
    if (enc == NULL || enc->tree == NULL) return NULL;
    SZ3HuffmanTreeI32 *tree = enc->tree;
    if (tree->qend < 2) return NULL;

    int i = 1;
    /* 弹出堆顶（当前最小频次节点）。 */
    SZ3HuffmanNodeI32 *n = tree->qq[1];
    tree->qend--;
    tree->qq[i] = tree->qq[tree->qend];

    int l;
    /* 下沉恢复最小堆结构。 */
    while ((l = (i << 1)) < tree->qend) {
        if (l + 1 < tree->qend && tree->qq[l + 1]->freq < tree->qq[l]->freq) l++;
        if (tree->qq[i]->freq > tree->qq[l]->freq) {
            SZ3HuffmanNodeI32 *p = tree->qq[i];
            tree->qq[i] = tree->qq[l];
            tree->qq[l] = p;
            i = l;
        } else {
            break;
        }
    }
    return n;
}

static inline void sz3_huffman_build_code_i32(SZ3HuffmanEncoderI32 *enc,
                                              SZ3HuffmanNodeI32 *node,
                                              int len,
                                              uint64_t out1,
                                              uint64_t out2) {
    if (enc == NULL || enc->tree == NULL || node == NULL) return;
    SZ3HuffmanTreeI32 *tree = enc->tree;
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
        sz3_huffman_build_code_i32(enc, node->left, len + 1, out1, 0);
        /* 向右分支追加 bit=1。 */
        out1 = out1 | 1U;
        sz3_huffman_build_code_i32(enc, node->right, len + 1, out1, 0);
    } else {
        if (len % 64 != 0) out2 = out2 << 1;
        sz3_huffman_build_code_i32(enc, node->left, len + 1, out1, out2);
        out2 = out2 | 1U;
        sz3_huffman_build_code_i32(enc, node->right, len + 1, out1, out2);
    }
}

static inline int sz3_huffman_init_i32(SZ3HuffmanEncoderI32 *enc, const int *input, size_t length) {
    if (enc == NULL || input == NULL || length == 0) return -1;

    /* 扫描输入，确定偏移量 offset(最小值) 与最大值。 */
    int max_v = input[0];
    enc->offset = input[0];
    for (size_t i = 1; i < length; i++) {
        if (input[i] > max_v) max_v = input[i];
        if (input[i] < enc->offset) enc->offset = input[i];
    }

    int state_num = (int)(max_v - enc->offset + 2);
    if (state_num <= 0) return -1;

    /* 按状态数创建 HuffmanTree 的节点池、堆和编码表。 */
    enc->tree = sz3_huffman_tree_i32_create(state_num);
    if (enc->tree == NULL) return -1;

    size_t *frequency_list = (size_t *)calloc((size_t)state_num, sizeof(size_t));
    if (frequency_list == NULL) {
        sz3_huffman_tree_i32_free(enc->tree);
        enc->tree = NULL;
        return -1;
    }

    for (size_t i = 0; i < length; i++) {
        int idx = input[i] - enc->offset;
        if (idx < 0 || idx >= state_num) {
            free(frequency_list);
            sz3_huffman_tree_i32_free(enc->tree);
            enc->tree = NULL;
            return -1;
        }
        frequency_list[idx] += 1U;
    }

    /* 将非零频次状态作为叶子节点插入最小堆。 */
    for (int i = 0; i < state_num; i++) {
        if (frequency_list[i] != 0) {
            sz3_huffman_qinsert_i32(enc, sz3_huffman_new_node_i32(enc, frequency_list[i], (int)i, NULL, NULL));
        }
    }

    free(frequency_list);

    /* 反复取出两个最小频次节点并合并，直到得到根节点。 */
    while (enc->tree->qend > 2) {
        SZ3HuffmanNodeI32 *left = sz3_huffman_qremove_i32(enc);
        SZ3HuffmanNodeI32 *right = sz3_huffman_qremove_i32(enc);
        sz3_huffman_qinsert_i32(enc, sz3_huffman_new_node_i32(enc, 0, 0, left, right));
    }

    /* 从根递归生成每个状态的 Huffman 码。 */
    enc->tree_root = enc->tree->qq[1];
    sz3_huffman_build_code_i32(enc, enc->tree_root, 0, 0, 0);
    return 0;
}

static inline int sz3_huffman_encoder_i32_preprocess_encode(SZ3HuffmanEncoderI32 *enc,
                                                            const int *bins,
                                                            size_t num_bin,
                                                            int state_num_hint) {
    (void)state_num_hint;
    if (enc == NULL || bins == NULL || num_bin == 0) return -1;

    /* 预处理前先释放历史树，避免跨批次污染。 */
    sz3_huffman_free_internal_i32(enc);
    enc->node_count = 0;
    if (sz3_huffman_init_i32(enc, bins, num_bin) != 0 || enc->tree == NULL) return -1;

    /* 统计有效叶子数，并换算序列化树节点总数。 */
    for (unsigned int i = 0; i < enc->tree->state_num; i++) {
        if (enc->tree->code[i] != NULL) enc->node_count++;
    }
    enc->node_count = enc->node_count * 2U - 1U;
    return 0;
}

static inline void sz3_huffman_encoder_i32_postprocess_encode(SZ3HuffmanEncoderI32 *enc) {
    /* 编码完成后释放 Huffman 内部内存。 */
    sz3_huffman_free_internal_i32(enc);
}

static inline size_t sz3_huffman_encoder_i32_size_est(const SZ3HuffmanEncoderI32 *enc) {
    if (enc == NULL) return 0;
    size_t node_count = enc->node_count;
    size_t b = (node_count <= 256U) ? sizeof(unsigned char)
                                    : ((node_count <= 65536U) ? sizeof(unsigned short) : sizeof(unsigned int));
    return 1U + 2U * node_count * b + node_count * sizeof(unsigned char) + node_count * sizeof(int) +
           sizeof(int) + sizeof(int) + sizeof(int);
}

static inline size_t sz3_huffman_encoder_i32_encode(const SZ3HuffmanEncoderI32 *enc,
                                                    const int *bins,
                                                    size_t num_bin,
                                                    unsigned char **bytes) {
    if (enc == NULL || bins == NULL || num_bin == 0 || bytes == NULL || *bytes == NULL) return 0;
    if (enc->tree == NULL || enc->tree->cout == NULL || enc->tree->code == NULL) return 0;

    size_t out_size = 0;
    unsigned char *p = *bytes + sizeof(size_t);
    int lack_bits = 0;

    for (size_t i = 0; i < num_bin; i++) {
        int state = (int)(bins[i] - enc->offset);
        if (state < 0 || (unsigned int)state >= enc->tree->state_num) return 0;
        if (enc->tree->code[state] == NULL) return 0;

        unsigned char bit_size = enc->tree->cout[state];
        unsigned char byte_size = 0;
        unsigned char byte_size_p;

        if (lack_bits == 0) {
            byte_size = (bit_size % 8U == 0U) ? (unsigned char)(bit_size / 8U) : (unsigned char)(bit_size / 8U + 1U);
            byte_size_p = (unsigned char)(bit_size / 8U);

            if (byte_size <= 8U) {
                sz3_dep_i32_int64_to_bytes_big_endian(p, enc->tree->code[state][0]);
                p += byte_size_p;
            } else {
                sz3_dep_i32_int64_to_bytes_big_endian(p, enc->tree->code[state][0]);
                p += 8;
                sz3_dep_i32_int64_to_bytes_big_endian(p, enc->tree->code[state][1]);
                p += (byte_size_p - 8U);
            }
            out_size += byte_size;
            lack_bits = (bit_size % 8U == 0U) ? 0 : (int)(8U - bit_size % 8U);
        } else {
            *p = (unsigned char)(*p | (unsigned char)(enc->tree->code[state][0] >> (64 - lack_bits)));
            if (lack_bits < bit_size) {
                p++;
                uint64_t new_code = enc->tree->code[state][0] << lack_bits;
                sz3_dep_i32_int64_to_bytes_big_endian(p, new_code);

                if (bit_size <= 64U) {
                    bit_size = (unsigned char)(bit_size - (unsigned char)lack_bits);
                    byte_size = (bit_size % 8U == 0U) ? (unsigned char)(bit_size / 8U)
                                                      : (unsigned char)(bit_size / 8U + 1U);
                    byte_size_p = (unsigned char)(bit_size / 8U);
                    p += byte_size_p;
                    out_size += byte_size;
                    lack_bits = (bit_size % 8U == 0U) ? 0 : (int)(8U - bit_size % 8U);
                } else {
                    byte_size_p = 7U;
                    p += byte_size_p;
                    out_size += byte_size;

                    bit_size = (unsigned char)(bit_size - 64U);
                    if (lack_bits < bit_size) {
                        *p = (unsigned char)(*p | (unsigned char)(enc->tree->code[state][0] >> (64 - lack_bits)));
                        p++;
                        new_code = enc->tree->code[state][1] << lack_bits;
                        sz3_dep_i32_int64_to_bytes_big_endian(p, new_code);
                        bit_size = (unsigned char)(bit_size - (unsigned char)lack_bits);
                        byte_size = (bit_size % 8U == 0U) ? (unsigned char)(bit_size / 8U)
                                                          : (unsigned char)(bit_size / 8U + 1U);
                        byte_size_p = (unsigned char)(bit_size / 8U);
                        p += byte_size_p;
                        out_size += byte_size;
                        lack_bits = (bit_size % 8U == 0U) ? 0 : (int)(8U - bit_size % 8U);
                    } else {
                        *p = (unsigned char)(*p | (unsigned char)(enc->tree->code[state][0] >> (64 - bit_size)));
                        lack_bits -= bit_size;
                    }
                }
            } else {
                lack_bits -= bit_size;
                if (lack_bits == 0) p++;
            }
        }
    }

    sz3_dep_i32_write_size_t(out_size, bytes);
    *bytes += out_size;
    return out_size;
}

static inline void sz3_huffman_encoder_i32_save(const SZ3HuffmanEncoderI32 *enc, unsigned char **c) {
    unsigned int total_size;
    if (enc == NULL || enc->tree == NULL || c == NULL || *c == NULL) return;

    /* 序列化头部：offset + node_count + state_num/2。 */
    memcpy(*c, &enc->offset, sizeof(int));
    *c += sizeof(int);
    sz3_dep_i32_int32_to_bytes_big_endian(*c, (int32_t)enc->node_count);
    *c += sizeof(int32_t);
    sz3_dep_i32_int32_to_bytes_big_endian(*c, (int32_t)(enc->tree->state_num / 2U));
    *c += sizeof(int32_t);

    /* 根据节点规模选择不同索引位宽，保持与 C++ 版本一致。 */
    if (enc->node_count <= 256U) {
        total_size = sz3_huffman_convert_tree_to_bytes_u8_i32((SZ3HuffmanEncoderI32 *)enc, enc->node_count, *c);
    } else if (enc->node_count <= 65536U) {
        total_size = sz3_huffman_convert_tree_to_bytes_u16_i32((SZ3HuffmanEncoderI32 *)enc, enc->node_count, *c);
    } else {
        total_size = sz3_huffman_convert_tree_to_bytes_u32_i32((SZ3HuffmanEncoderI32 *)enc, enc->node_count, *c);
    }
    *c += total_size;
}

static inline void sz3_huffman_encoder_i32_preprocess_decode(SZ3HuffmanEncoderI32 *enc) {
    /* 与 C++ 语义对齐：该阶段无需额外处理。 */
    (void)enc;
}

static inline int sz3_huffman_encoder_i32_decode(SZ3HuffmanEncoderI32 *enc,
                                                 const unsigned char **bytes,
                                                 size_t target_length,
                                                 int *out_values) {
    size_t i;
    size_t encoded_length = 0;
    size_t bit_pos = 0;
    size_t out_count = 0;
    SZ3HuffmanNodeI32 *root;
    SZ3HuffmanNodeI32 *cur;

    if (enc == NULL || bytes == NULL || *bytes == NULL || out_values == NULL) return -1;
    if (target_length == 0) return 0;
    if (enc->tree_root == NULL) return -1;

    root = enc->tree_root;
    cur = root;
    sz3_dep_i32_read_size_t(&encoded_length, bytes);

    /* 常量块快捷路径：根节点即叶子时直接填充。 */
    if (root->is_leaf) {
        for (i = 0; i < target_length; i++) {
            out_values[i] = root->symbol + enc->offset;
        }
        *bytes += encoded_length;
        return 0;
    }

    /* 位流按 bit 遍历 Huffman 树，遇叶子即输出一个状态。 */
    while (out_count < target_length) {
        size_t byte_index = bit_pos >> 3;
        int r = (int)(bit_pos & 7U);
        unsigned char bit = (unsigned char)(((*bytes)[byte_index] >> (7 - r)) & 0x01U);
        cur = (bit == 0U) ? cur->left : cur->right;
        if (cur == NULL) return -1;
        if (cur->is_leaf) {
            out_values[out_count++] = cur->symbol + enc->offset;
            cur = root;
        }
        bit_pos++;
    }
    *bytes += encoded_length;
    return 0;
}

static inline void sz3_huffman_encoder_i32_postprocess_decode(SZ3HuffmanEncoderI32 *enc) {
    /* 解码后与 C++ postprocess_decode 对齐：释放树资源。 */
    sz3_huffman_free_internal_i32(enc);
}

static inline int sz3_huffman_encoder_i32_load(SZ3HuffmanEncoderI32 *enc,
                                                const unsigned char **c,
                                                size_t *remaining_length) {
    int state_num_half;
    size_t encode_start_index;
    if (enc == NULL || c == NULL || *c == NULL || remaining_length == NULL) return -1;

    sz3_huffman_free_internal_i32(enc);
    if (*remaining_length < sizeof(int)) return -1;
    memcpy(&enc->offset, *c, sizeof(int));
    *c += sizeof(int);
    *remaining_length -= sizeof(int);
    if (*remaining_length < 2U * sizeof(int32_t)) return -1;
    enc->node_count = (unsigned int)sz3_dep_i32_bytes_to_int32_big_endian(*c);
    *c += sizeof(int32_t);
    *remaining_length -= sizeof(int32_t);
    state_num_half = sz3_dep_i32_bytes_to_int32_big_endian(*c);
    *c += sizeof(int32_t);
    *remaining_length -= sizeof(int32_t);

    if (enc->node_count <= 256U) {
        encode_start_index = 1U + 3U * enc->node_count * sizeof(uint8_t) + enc->node_count * sizeof(int);
    } else if (enc->node_count <= 65536U) {
        encode_start_index = 1U + 2U * enc->node_count * sizeof(uint16_t) + enc->node_count * sizeof(unsigned char) +
                             enc->node_count * sizeof(int);
    } else {
        encode_start_index = 1U + 2U * enc->node_count * sizeof(uint32_t) + enc->node_count * sizeof(unsigned char) +
                             enc->node_count * sizeof(int);
    }
    if (*remaining_length < encode_start_index) return -1;

    enc->tree = sz3_huffman_tree_i32_create(state_num_half * 2);
    if (enc->tree == NULL) return -1;
    enc->tree_root = sz3_huffman_reconstruct_tree_from_bytes_any_states_i32(enc, *c, enc->node_count);
    if (enc->tree_root == NULL) {
        sz3_huffman_free_internal_i32(enc);
        enc->loaded = false;
        return -1;
    }

    *c += encode_start_index;
    *remaining_length -= encode_start_index;
    enc->loaded = true;
    return 0;
}

static inline bool sz3_huffman_encoder_i32_is_loaded(const SZ3HuffmanEncoderI32 *enc) {
    return (enc != NULL) ? enc->loaded : false;
}

static inline SZ3HuffmanNodeI32 *sz3_huffman_reconstruct_tree_from_bytes_any_states_i32(SZ3HuffmanEncoderI32 *enc,
                                                                                          const unsigned char *bytes,
                                                                                          unsigned int node_count) {
    if (enc == NULL || bytes == NULL || node_count == 0U) return NULL;
    if (node_count <= 256U) {
        uint8_t *L = (uint8_t *)calloc(node_count, sizeof(uint8_t));
        uint8_t *R = (uint8_t *)calloc(node_count, sizeof(uint8_t));
        int *C = (int *)calloc(node_count, sizeof(int));
        unsigned char *t = (unsigned char *)calloc(node_count, sizeof(unsigned char));
        SZ3HuffmanNodeI32 *root;
        if (L == NULL || R == NULL || C == NULL || t == NULL) {
            free(L); free(R); free(C); free(t);
            return NULL;
        }
        memcpy(L, bytes + 1, node_count * sizeof(uint8_t));
        memcpy(R, bytes + 1 + node_count * sizeof(uint8_t), node_count * sizeof(uint8_t));
        memcpy(C, bytes + 1 + 2U * node_count * sizeof(uint8_t), node_count * sizeof(int));
        memcpy(t, bytes + 1 + 2U * node_count * sizeof(uint8_t) + node_count * sizeof(int),
               node_count * sizeof(unsigned char));
        root = sz3_huffman_new_node2_i32(enc, C[0], t[0]);
        sz3_huffman_unpad_tree_u8_i32(enc, L, R, C, t, 0U, root);
        free(L); free(R); free(C); free(t);
        return root;
    } else if (node_count <= 65536U) {
        uint16_t *L = (uint16_t *)calloc(node_count, sizeof(uint16_t));
        uint16_t *R = (uint16_t *)calloc(node_count, sizeof(uint16_t));
        int *C = (int *)calloc(node_count, sizeof(int));
        unsigned char *t = (unsigned char *)calloc(node_count, sizeof(unsigned char));
        SZ3HuffmanNodeI32 *root;
        if (L == NULL || R == NULL || C == NULL || t == NULL) {
            free(L); free(R); free(C); free(t);
            return NULL;
        }
        memcpy(L, bytes + 1, node_count * sizeof(uint16_t));
        memcpy(R, bytes + 1 + node_count * sizeof(uint16_t), node_count * sizeof(uint16_t));
        memcpy(C, bytes + 1 + 2U * node_count * sizeof(uint16_t), node_count * sizeof(int));
        memcpy(t, bytes + 1 + 2U * node_count * sizeof(uint16_t) + node_count * sizeof(int),
               node_count * sizeof(unsigned char));
        root = sz3_huffman_new_node2_i32(enc, C[0], t[0]);
        sz3_huffman_unpad_tree_u16_i32(enc, L, R, C, t, 0U, root);
        free(L); free(R); free(C); free(t);
        return root;
    } else {
        uint32_t *L = (uint32_t *)calloc(node_count, sizeof(uint32_t));
        uint32_t *R = (uint32_t *)calloc(node_count, sizeof(uint32_t));
        int *C = (int *)calloc(node_count, sizeof(int));
        unsigned char *t = (unsigned char *)calloc(node_count, sizeof(unsigned char));
        SZ3HuffmanNodeI32 *root;
        if (L == NULL || R == NULL || C == NULL || t == NULL) {
            free(L); free(R); free(C); free(t);
            return NULL;
        }
        memcpy(L, bytes + 1, node_count * sizeof(uint32_t));
        memcpy(R, bytes + 1 + node_count * sizeof(uint32_t), node_count * sizeof(uint32_t));
        memcpy(C, bytes + 1 + 2U * node_count * sizeof(uint32_t), node_count * sizeof(int));
        memcpy(t, bytes + 1 + 2U * node_count * sizeof(uint32_t) + node_count * sizeof(int),
               node_count * sizeof(unsigned char));
        root = sz3_huffman_new_node2_i32(enc, C[0], t[0]);
        sz3_huffman_unpad_tree_u32_i32(enc, L, R, C, t, 0U, root);
        free(L); free(R); free(C); free(t);
        return root;
    }
}

static inline SZ3HuffmanNodeI32 *sz3_huffman_new_node2_i32(SZ3HuffmanEncoderI32 *enc,
                                                            int symbol,
                                                            unsigned char is_leaf) {
    if (enc == NULL || enc->tree == NULL || enc->tree->pool == NULL) return NULL;
    SZ3HuffmanNodeI32 *n = enc->tree->pool + enc->tree->n_nodes++;
    n->left = NULL;
    n->right = NULL;
    n->freq = 0U;
    n->symbol = symbol;
    n->is_leaf = is_leaf;
    return n;
}

static inline void sz3_huffman_pad_tree_u8_i32(SZ3HuffmanEncoderI32 *enc,
                                                uint8_t *L,
                                                uint8_t *R,
                                                int *C,
                                                unsigned char *t,
                                                unsigned int i,
                                                SZ3HuffmanNodeI32 *root) {
    if (enc == NULL || enc->tree == NULL || root == NULL) return;
    C[i] = root->symbol;
    t[i] = root->is_leaf;
    if (root->left != NULL) {
        enc->tree->n_inode++;
        L[i] = (uint8_t)enc->tree->n_inode;
        sz3_huffman_pad_tree_u8_i32(enc, L, R, C, t, (unsigned int)enc->tree->n_inode, root->left);
    }
    if (root->right != NULL) {
        enc->tree->n_inode++;
        R[i] = (uint8_t)enc->tree->n_inode;
        sz3_huffman_pad_tree_u8_i32(enc, L, R, C, t, (unsigned int)enc->tree->n_inode, root->right);
    }
}

static inline void sz3_huffman_pad_tree_u16_i32(SZ3HuffmanEncoderI32 *enc,
                                                 uint16_t *L,
                                                 uint16_t *R,
                                                 int *C,
                                                 unsigned char *t,
                                                 unsigned int i,
                                                 SZ3HuffmanNodeI32 *root) {
    if (enc == NULL || enc->tree == NULL || root == NULL) return;
    C[i] = root->symbol;
    t[i] = root->is_leaf;
    if (root->left != NULL) {
        enc->tree->n_inode++;
        L[i] = (uint16_t)enc->tree->n_inode;
        sz3_huffman_pad_tree_u16_i32(enc, L, R, C, t, (unsigned int)enc->tree->n_inode, root->left);
    }
    if (root->right != NULL) {
        enc->tree->n_inode++;
        R[i] = (uint16_t)enc->tree->n_inode;
        sz3_huffman_pad_tree_u16_i32(enc, L, R, C, t, (unsigned int)enc->tree->n_inode, root->right);
    }
}

static inline void sz3_huffman_pad_tree_u32_i32(SZ3HuffmanEncoderI32 *enc,
                                                 uint32_t *L,
                                                 uint32_t *R,
                                                 int *C,
                                                 unsigned char *t,
                                                 unsigned int i,
                                                 SZ3HuffmanNodeI32 *root) {
    if (enc == NULL || enc->tree == NULL || root == NULL) return;
    C[i] = root->symbol;
    t[i] = root->is_leaf;
    if (root->left != NULL) {
        enc->tree->n_inode++;
        L[i] = (uint32_t)enc->tree->n_inode;
        sz3_huffman_pad_tree_u32_i32(enc, L, R, C, t, (unsigned int)enc->tree->n_inode, root->left);
    }
    if (root->right != NULL) {
        enc->tree->n_inode++;
        R[i] = (uint32_t)enc->tree->n_inode;
        sz3_huffman_pad_tree_u32_i32(enc, L, R, C, t, (unsigned int)enc->tree->n_inode, root->right);
    }
}

static inline void sz3_huffman_unpad_tree_u8_i32(SZ3HuffmanEncoderI32 *enc,
                                                  const uint8_t *L,
                                                  const uint8_t *R,
                                                  const int *C,
                                                  const unsigned char *t,
                                                  unsigned int i,
                                                  SZ3HuffmanNodeI32 *root) {
    uint8_t l, r;
    if (enc == NULL || enc->tree == NULL || root == NULL) return;
    if (root->is_leaf != 0U) return;
    l = L[i];
    if (l != 0U) {
        SZ3HuffmanNodeI32 *lnode = sz3_huffman_new_node2_i32(enc, C[l], t[l]);
        root->left = lnode;
        sz3_huffman_unpad_tree_u8_i32(enc, L, R, C, t, l, lnode);
    }
    r = R[i];
    if (r != 0U) {
        SZ3HuffmanNodeI32 *rnode = sz3_huffman_new_node2_i32(enc, C[r], t[r]);
        root->right = rnode;
        sz3_huffman_unpad_tree_u8_i32(enc, L, R, C, t, r, rnode);
    }
}

static inline void sz3_huffman_unpad_tree_u16_i32(SZ3HuffmanEncoderI32 *enc,
                                                   const uint16_t *L,
                                                   const uint16_t *R,
                                                   const int *C,
                                                   const unsigned char *t,
                                                   unsigned int i,
                                                   SZ3HuffmanNodeI32 *root) {
    uint16_t l, r;
    if (enc == NULL || enc->tree == NULL || root == NULL) return;
    if (root->is_leaf != 0U) return;
    l = L[i];
    if (l != 0U) {
        SZ3HuffmanNodeI32 *lnode = sz3_huffman_new_node2_i32(enc, C[l], t[l]);
        root->left = lnode;
        sz3_huffman_unpad_tree_u16_i32(enc, L, R, C, t, l, lnode);
    }
    r = R[i];
    if (r != 0U) {
        SZ3HuffmanNodeI32 *rnode = sz3_huffman_new_node2_i32(enc, C[r], t[r]);
        root->right = rnode;
        sz3_huffman_unpad_tree_u16_i32(enc, L, R, C, t, r, rnode);
    }
}

static inline void sz3_huffman_unpad_tree_u32_i32(SZ3HuffmanEncoderI32 *enc,
                                                   const uint32_t *L,
                                                   const uint32_t *R,
                                                   const int *C,
                                                   const unsigned char *t,
                                                   unsigned int i,
                                                   SZ3HuffmanNodeI32 *root) {
    uint32_t l, r;
    if (enc == NULL || enc->tree == NULL || root == NULL) return;
    if (root->is_leaf != 0U) return;
    l = L[i];
    if (l != 0U) {
        SZ3HuffmanNodeI32 *lnode = sz3_huffman_new_node2_i32(enc, C[l], t[l]);
        root->left = lnode;
        sz3_huffman_unpad_tree_u32_i32(enc, L, R, C, t, l, lnode);
    }
    r = R[i];
    if (r != 0U) {
        SZ3HuffmanNodeI32 *rnode = sz3_huffman_new_node2_i32(enc, C[r], t[r]);
        root->right = rnode;
        sz3_huffman_unpad_tree_u32_i32(enc, L, R, C, t, r, rnode);
    }
}

static inline unsigned int sz3_huffman_convert_tree_to_bytes_u8_i32(SZ3HuffmanEncoderI32 *enc,
                                                                     unsigned int node_count,
                                                                     unsigned char *out) {
    uint8_t *L;
    uint8_t *R;
    int *C;
    unsigned char *t;
    unsigned int total_size;
    if (enc == NULL || enc->tree == NULL || out == NULL || node_count == 0U) return 0U;
    L = (uint8_t *)calloc(node_count, sizeof(uint8_t));
    R = (uint8_t *)calloc(node_count, sizeof(uint8_t));
    C = (int *)calloc(node_count, sizeof(int));
    t = (unsigned char *)calloc(node_count, sizeof(unsigned char));
    if (L == NULL || R == NULL || C == NULL || t == NULL) {
        free(L); free(R); free(C); free(t);
        return 0U;
    }
    enc->tree->n_inode = 0;
    sz3_huffman_pad_tree_u8_i32(enc, L, R, C, t, 0U, enc->tree->qq[1]);
    total_size = 1U + 2U * node_count * sizeof(uint8_t) + node_count * sizeof(int) + node_count * sizeof(unsigned char);
    out[0] = enc->sys_endian_type;
    memcpy(out + 1, L, node_count * sizeof(uint8_t));
    memcpy(out + 1 + node_count * sizeof(uint8_t), R, node_count * sizeof(uint8_t));
    memcpy(out + 1 + 2U * node_count * sizeof(uint8_t), C, node_count * sizeof(int));
    memcpy(out + 1 + 2U * node_count * sizeof(uint8_t) + node_count * sizeof(int), t,
           node_count * sizeof(unsigned char));
    free(L); free(R); free(C); free(t);
    return total_size;
}

static inline unsigned int sz3_huffman_convert_tree_to_bytes_u16_i32(SZ3HuffmanEncoderI32 *enc,
                                                                      unsigned int node_count,
                                                                      unsigned char *out) {
    uint16_t *L;
    uint16_t *R;
    int *C;
    unsigned char *t;
    unsigned int total_size;
    if (enc == NULL || enc->tree == NULL || out == NULL || node_count == 0U) return 0U;
    L = (uint16_t *)calloc(node_count, sizeof(uint16_t));
    R = (uint16_t *)calloc(node_count, sizeof(uint16_t));
    C = (int *)calloc(node_count, sizeof(int));
    t = (unsigned char *)calloc(node_count, sizeof(unsigned char));
    if (L == NULL || R == NULL || C == NULL || t == NULL) {
        free(L); free(R); free(C); free(t);
        return 0U;
    }
    enc->tree->n_inode = 0;
    sz3_huffman_pad_tree_u16_i32(enc, L, R, C, t, 0U, enc->tree->qq[1]);
    total_size = 1U + 2U * node_count * sizeof(uint16_t) + node_count * sizeof(int) + node_count * sizeof(unsigned char);
    out[0] = enc->sys_endian_type;
    memcpy(out + 1, L, node_count * sizeof(uint16_t));
    memcpy(out + 1 + node_count * sizeof(uint16_t), R, node_count * sizeof(uint16_t));
    memcpy(out + 1 + 2U * node_count * sizeof(uint16_t), C, node_count * sizeof(int));
    memcpy(out + 1 + 2U * node_count * sizeof(uint16_t) + node_count * sizeof(int), t,
           node_count * sizeof(unsigned char));
    free(L); free(R); free(C); free(t);
    return total_size;
}

static inline unsigned int sz3_huffman_convert_tree_to_bytes_u32_i32(SZ3HuffmanEncoderI32 *enc,
                                                                      unsigned int node_count,
                                                                      unsigned char *out) {
    uint32_t *L;
    uint32_t *R;
    int *C;
    unsigned char *t;
    unsigned int total_size;
    if (enc == NULL || enc->tree == NULL || out == NULL || node_count == 0U) return 0U;
    L = (uint32_t *)calloc(node_count, sizeof(uint32_t));
    R = (uint32_t *)calloc(node_count, sizeof(uint32_t));
    C = (int *)calloc(node_count, sizeof(int));
    t = (unsigned char *)calloc(node_count, sizeof(unsigned char));
    if (L == NULL || R == NULL || C == NULL || t == NULL) {
        free(L); free(R); free(C); free(t);
        return 0U;
    }
    enc->tree->n_inode = 0;
    sz3_huffman_pad_tree_u32_i32(enc, L, R, C, t, 0U, enc->tree->qq[1]);
    total_size = 1U + 2U * node_count * sizeof(uint32_t) + node_count * sizeof(int) + node_count * sizeof(unsigned char);
    out[0] = enc->sys_endian_type;
    memcpy(out + 1, L, node_count * sizeof(uint32_t));
    memcpy(out + 1 + node_count * sizeof(uint32_t), R, node_count * sizeof(uint32_t));
    memcpy(out + 1 + 2U * node_count * sizeof(uint32_t), C, node_count * sizeof(int));
    memcpy(out + 1 + 2U * node_count * sizeof(uint32_t) + node_count * sizeof(int), t,
           node_count * sizeof(unsigned char));
    free(L); free(R); free(C); free(t);
    return total_size;
}

static inline void sz3_huffman_free_internal_i32(SZ3HuffmanEncoderI32 *enc) {
    if (enc == NULL) return;
    if (enc->tree != NULL) {
        /* 统一回收树池/堆/码表，重置指针状态。 */
        sz3_huffman_tree_i32_free(enc->tree);
        enc->tree = NULL;
    }
    enc->tree_root = NULL;
    enc->node_count = 0;
}

static inline void sz3_huffman_encoder_i32_destroy(SZ3HuffmanEncoderI32 *enc) {
    if (enc == NULL) return;
    sz3_huffman_free_internal_i32(enc);
    enc->loaded = false;
}

#ifdef __cplusplus
}
#endif

#endif /* SZ3_HUFFMAN_ENCODER_I32_C_H */
