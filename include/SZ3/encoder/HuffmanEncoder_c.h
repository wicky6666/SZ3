#ifndef SZ3_HUFFMAN_ENCODER_C_H
#define SZ3_HUFFMAN_ENCODER_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/* 初始化编码器上下文（对标 C++ 构造函数） */
void sz3_huffman_encoder_f32_init(SZ3HuffmanEncoderF32 *enc);

/* 释放编码器内部资源（对标 C++ 析构 + SZ_FreeHuffman） */
void sz3_huffman_encoder_f32_destroy(SZ3HuffmanEncoderF32 *enc);

/* 创建 HuffmanTree（对标 createHuffmanTree） */
SZ3HuffmanTreeF32 *sz3_huffman_tree_f32_create(int state_num);

/* 释放 HuffmanTree（C 风格公共接口） */
void sz3_huffman_tree_f32_free(SZ3HuffmanTreeF32 *tree);

/*
 * ===================== 编码流程函数（对标 EncoderInterface） =====================
 */

/* 预处理：根据输入数据建立 Huffman 树与编码表（对标 preprocess_encode） */
int sz3_huffman_encoder_f32_preprocess_encode(SZ3HuffmanEncoderF32 *enc,
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
void sz3_huffman_encoder_f32_postprocess_encode(SZ3HuffmanEncoderF32 *enc);

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

SZ3HuffmanNodeF32 *sz3_huffman_new_node_f32(SZ3HuffmanEncoderF32 *enc,
                                            size_t freq,
                                            float symbol,
                                            SZ3HuffmanNodeF32 *left,
                                            SZ3HuffmanNodeF32 *right);

SZ3HuffmanNodeF32 *sz3_huffman_new_node2_f32(SZ3HuffmanEncoderF32 *enc,
                                             float symbol,
                                             unsigned char is_leaf);

void sz3_huffman_qinsert_f32(SZ3HuffmanEncoderF32 *enc, SZ3HuffmanNodeF32 *node);
SZ3HuffmanNodeF32 *sz3_huffman_qremove_f32(SZ3HuffmanEncoderF32 *enc);

void sz3_huffman_build_code_f32(SZ3HuffmanEncoderF32 *enc,
                                SZ3HuffmanNodeF32 *node,
                                int len,
                                uint64_t out1,
                                uint64_t out2);

int sz3_huffman_init_f32(SZ3HuffmanEncoderF32 *enc, const float *input, size_t length);

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
void sz3_huffman_free_internal_f32(SZ3HuffmanEncoderF32 *enc);

#ifdef __cplusplus
}
#endif

#endif /* SZ3_HUFFMAN_ENCODER_C_H */
