/*
 * sha256.h - SHA-256 哈希算法头文件
 *
 * 功能概述：
 *   实现 SHA-256（Secure Hash Algorithm 256-bit）密码学哈希函数。
 *   SHA-256 是 NIST 发布的 SHA-2 系列算法之一，广泛用于：
 *   - 密码存储（本项目用途：salt + SHA-256 存储用户密码）
 *   - 数字签名
 *   - 数据完整性校验
 *   - 区块链（比特币的工作量证明）
 *
 * 算法特性：
 *   - 输入：任意长度的消息
 *   - 输出：固定 256 位（32 字节 / 64 个十六进制字符）的哈希值
 *   - 单向性：从哈希值无法反推原始数据
 *   - 抗碰撞性：几乎不可能找到两个不同的输入产生相同的哈希值
 *   - 雪崩效应：输入微小变化导致输出完全不同
 *
 * 使用方式：
 *   SHA256_CTX ctx;              // 1. 创建上下文
 *   sha256_init(&ctx);           // 2. 初始化
 *   sha256_update(&ctx, data, len); // 3. 填入数据（可多次调用）
 *   uint8_t hash[32];            // 4. 准备输出缓冲区
 *   sha256_final(&ctx, hash);    // 5. 计算最终哈希值
 *
 * 本项目中的使用：
 *   在 ChatServer.cpp 的 db_register() 和 db_login() 中，
 *   用于计算 salt+password 的哈希值，实现密码的安全存储和验证。
 *   具体流程：sha256_hash() 封装了上述5步，输出64字符的十六进制字符串。
 */

#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <string.h>

/* 支持 C++ 编译器调用（ChatServer.cpp 是 C++ 文件） */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * SHA256_CTX - SHA-256 计算上下文
 *
 * 保存 SHA-256 计算过程中的中间状态，支持分块（streaming）处理：
 *   - 可以多次调用 sha256_update() 分批填入数据
 *   - 最后调用 sha256_final() 得到最终哈希值
 *
 * 字段说明：
 *   data[64]  - 当前数据块缓冲区，SHA-256 每次处理 512 位（64 字节）
 *   datalen   - 当前数据块中已填入的字节数（0~63）
 *   bitlen    - 已处理的总比特数（用于最终填充时记录消息长度）
 *   state[8]  - 8 个 32 位工作变量（a~h），即哈希计算的中间状态
 *               初始值为 SHA-256 标准定义的常量（素数平方根的小数部分）
 */
typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

/*
 * sha256_init() - 初始化 SHA-256 上下文
 *
 * 将 state 设置为 SHA-256 标准定义的初始哈希值（8 个 32 位常量），
 * 这些常量是前 8 个素数平方根的小数部分的前 32 位：
 *   H0 = 0x6a09e667 (√2)
 *   H1 = 0xbb67ae85 (√3)
 *   H2 = 0x3c6ef372 (√5)
 *   H3 = 0xa54ff53a (√7)
 *   H4 = 0x510e527f (√11)
 *   H5 = 0x9b05688c (√13)
 *   H6 = 0x1f83d9ab (√17)
 *   H7 = 0x5be0cd19 (√19)
 */
void sha256_init(SHA256_CTX *ctx);

/*
 * sha256_update() - 向 SHA-256 上下文中填入数据
 *
 * 可以多次调用，数据会被累积处理。
 * 每当 data[64] 缓冲区填满（64 字节 = 512 位）时，
 * 自动调用 sha256_transform() 处理该数据块并更新 state。
 *
 * 参数：
 *   ctx  - SHA-256 上下文
 *   data - 输入数据
 *   len  - 输入数据长度（字节）
 */
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);

/*
 * sha256_final() - 计算最终哈希值
 *
 * 完成哈希计算的最后步骤：
 *   1. 添加填充位（1 后跟若干 0，使数据长度 ≡ 448 mod 512）
 *   2. 追加原始消息长度（64 位大端序整数）
 *   3. 处理最后一个（或两个）数据块
 *   4. 将 8 个 32 位 state 值拼接为 32 字节的哈希值输出
 *
 * 参数：
 *   ctx  - SHA-256 上下文
 *   hash - 输出缓冲区，至少 32 字节，存储最终的 256 位哈希值
 */
void sha256_final(SHA256_CTX *ctx, uint8_t hash[32]);

#ifdef __cplusplus
}
#endif

#endif
