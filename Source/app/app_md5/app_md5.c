#include "app_md5.h"
#include "../Source/bsp/bsp_flash/bsp_flash.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void MD5_Transform(uint32_t state[4], const uint8_t block[64]);

static const uint8_t PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define F(x, y, z)        (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z)        (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z)        ((x) ^ (y) ^ (z))
#define I(x, y, z)        ((y) ^ ((x) | (~z)))
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define FF(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s));                    \
        (a) += (b);                                     \
    }
#define GG(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s));                    \
        (a) += (b);                                     \
    }
#define HH(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s));                    \
        (a) += (b);                                     \
    }
#define II(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s));                    \
        (a) += (b);                                     \
    }

void MD5_Init(MD5_CTX *ctx)
{
    ctx->count[0] = ctx->count[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

void MD5_Update(MD5_CTX *ctx, const uint8_t *input, uint32_t inputLen)
{
    uint32_t i, index, partLen;
    index = (uint32_t)((ctx->count[0] >> 3) & 0x3F);
    if ((ctx->count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3)) ctx->count[1]++;
    ctx->count[1] += ((uint32_t)inputLen >> 29);
    partLen = 64 - index;
    if (inputLen >= partLen) {
        memcpy(&ctx->buffer[index], input, partLen);
        MD5_Transform(ctx->state, ctx->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64) MD5_Transform(ctx->state, &input[i]);
        index = 0;
    } else
        i = 0;
    memcpy(&ctx->buffer[index], &input[i], inputLen - i);
}

void MD5_Final(uint8_t digest[16], MD5_CTX *ctx)
{
    uint8_t bits[8];
    uint32_t index, padLen;

    // 将总比特数存入 bits 数组（小端序）
    // 注意：这里必须是 count[0] 和 count[1]
    uint32_t low  = ctx->count[0];
    uint32_t high = ctx->count[1];

    bits[0] = (uint8_t)(low & 0xff);
    bits[1] = (uint8_t)((low >> 8) & 0xff);
    bits[2] = (uint8_t)((low >> 16) & 0xff);
    bits[3] = (uint8_t)((low >> 24) & 0xff);
    bits[4] = (uint8_t)(high & 0xff);
    bits[5] = (uint8_t)((high >> 8) & 0xff);
    bits[6] = (uint8_t)((high >> 16) & 0xff);
    bits[7] = (uint8_t)((high >> 24) & 0xff);

    index  = (uint32_t)((ctx->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);

    // 填充 0x80 和 0
    MD5_Update(ctx, PADDING, padLen);
    // 填充长度信息（这是最关键的 8 字节）
    MD5_Update(ctx, bits, 8);

    // 输出最终状态
    for (int i = 0, j = 0; i < 4; i++, j += 4) {
        digest[j]     = (uint8_t)(ctx->state[i] & 0xff);
        digest[j + 1] = (uint8_t)((ctx->state[i] >> 8) & 0xff);
        digest[j + 2] = (uint8_t)((ctx->state[i] >> 16) & 0xff);
        digest[j + 3] = (uint8_t)((ctx->state[i] >> 24) & 0xff);
    }
}

static void MD5_Transform(uint32_t state[4], const uint8_t block[64])
{
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    // 将 64 字节分组转换成 16 个 uint32_t (小端模式)
    for (int i = 0, j = 0; i < 16; i++, j += 4) {
        x[i] = ((uint32_t)block[j]) |
               (((uint32_t)block[j + 1]) << 8) |
               (((uint32_t)block[j + 2]) << 16) |
               (((uint32_t)block[j + 3]) << 24);
    }

    // Round 1
    FF(a, b, c, d, x[0], S11, 0xd76aa478);
    FF(d, a, b, c, x[1], S12, 0xe8c7b756);
    FF(c, d, a, b, x[2], S13, 0x242070db);
    FF(b, c, d, a, x[3], S14, 0xc1bdceee);
    FF(a, b, c, d, x[4], S11, 0xf57c0faf);
    FF(d, a, b, c, x[5], S12, 0x4787c62a);
    FF(c, d, a, b, x[6], S13, 0xa8304613);
    FF(b, c, d, a, x[7], S14, 0xfd469501);
    FF(a, b, c, d, x[8], S11, 0x698098d8);
    FF(d, a, b, c, x[9], S12, 0x8b44f7af);
    FF(c, d, a, b, x[10], S13, 0xffff5bb1);
    FF(b, c, d, a, x[11], S14, 0x895cd7be);
    FF(a, b, c, d, x[12], S11, 0x6b901122);
    FF(d, a, b, c, x[13], S12, 0xfd987193);
    FF(c, d, a, b, x[14], S13, 0xa679438e);
    FF(b, c, d, a, x[15], S14, 0x49b40821);

    // Round 2
    GG(a, b, c, d, x[1], S21, 0xf61e2562);
    GG(d, a, b, c, x[6], S22, 0xc040b340);
    GG(c, d, a, b, x[11], S23, 0x265e5a51);
    GG(b, c, d, a, x[0], S24, 0xe9b6c7aa);
    GG(a, b, c, d, x[5], S21, 0xd62f105d);
    GG(d, a, b, c, x[10], S22, 0x02441453);
    GG(c, d, a, b, x[15], S23, 0xd8a1e681);
    GG(b, c, d, a, x[4], S24, 0xe7d3fbc8);
    GG(a, b, c, d, x[9], S21, 0x21e1cde6);
    GG(d, a, b, c, x[14], S22, 0xc33707d6);
    GG(c, d, a, b, x[3], S23, 0xf4d50d87);
    GG(b, c, d, a, x[8], S24, 0x455a14ed);
    GG(a, b, c, d, x[13], S21, 0xa9e3e905);
    GG(d, a, b, c, x[2], S22, 0xfcefa3f8);
    GG(c, d, a, b, x[7], S23, 0x676f02d9);
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a);

    // Round 3
    HH(a, b, c, d, x[5], S31, 0xfffa3942);
    HH(d, a, b, c, x[8], S32, 0x8771f681);
    HH(c, d, a, b, x[11], S33, 0x6d9d6122);
    HH(b, c, d, a, x[14], S34, 0xfde5380c);
    HH(a, b, c, d, x[1], S31, 0xa4beea44);
    HH(d, a, b, c, x[4], S32, 0x4bdecfa9);
    HH(c, d, a, b, x[7], S33, 0xf6bb4b60);
    HH(b, c, d, a, x[10], S34, 0xbebfbc70);
    HH(a, b, c, d, x[13], S31, 0x289b7ec6);
    HH(d, a, b, c, x[0], S32, 0xeaa127fa);
    HH(c, d, a, b, x[3], S33, 0xd4ef3085);
    HH(b, c, d, a, x[6], S34, 0x04881d05);
    HH(a, b, c, d, x[9], S31, 0xd9d4d039);
    HH(d, a, b, c, x[12], S32, 0xe6db99e5);
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8);
    HH(b, c, d, a, x[2], S34, 0xc4ac5665);

    // Round 4
    II(a, b, c, d, x[0], S41, 0xf4292244);
    II(d, a, b, c, x[7], S42, 0x432aff97);
    II(c, d, a, b, x[14], S43, 0xab9423a7);
    II(b, c, d, a, x[5], S44, 0xfc93a039);
    II(a, b, c, d, x[12], S41, 0x655b59c3);
    II(d, a, b, c, x[3], S42, 0x8f0ccc92);
    II(c, d, a, b, x[10], S43, 0xffeff47d);
    II(b, c, d, a, x[1], S44, 0x85845dd1);
    II(a, b, c, d, x[8], S41, 0x6fa87e4f);
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0);
    II(c, d, a, b, x[6], S43, 0xa3014314);
    II(b, c, d, a, x[13], S44, 0x4e0811a1);
    II(a, b, c, d, x[4], S41, 0xf7537e82);
    II(d, a, b, c, x[11], S42, 0xbd3af235);
    II(c, d, a, b, x[2], S43, 0x2ad7d2bb);
    II(b, c, d, a, x[9], S44, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}
#define SECTOR_SIZE 2048 // 每次读取 2048 字节

int Calculate_File_MD5(uint32_t flash_start_addr, uint32_t file_length, uint8_t out_md5[16])
{
    MD5_CTX md5_ctx;
    uint32_t sector_buf[FLASH_PAGE_SIZE / 4];
    uint32_t total_processed = 0;
    uint32_t bytes_remaining = file_length;
    uint32_t current_addr    = flash_start_addr;

    MD5_Init(&md5_ctx);

    while (bytes_remaining > 0) {
        uint32_t read_len         = (bytes_remaining >= FLASH_PAGE_SIZE) ? FLASH_PAGE_SIZE : bytes_remaining;
        uint32_t aligned_read_len = (read_len + 3) & ~3;

        memset(sector_buf, 0, sizeof(sector_buf));
        if (app_flash_read_word(current_addr, sector_buf, aligned_read_len) != FMC_READY) {
            printf("\nFlash Read Error at 0x%08X\n", current_addr);
            return -1;
        }

        MD5_Update(&md5_ctx, (const uint8_t *)sector_buf, read_len);

        total_processed += read_len;
        bytes_remaining -= read_len;
        current_addr += read_len;
    }

    MD5_Final(out_md5, &md5_ctx);

    printf("Processed: %u bytes (expected %u)\n", total_processed, file_length);
    printf("MD5: ");
    for (int i = 0; i < 16; i++) printf("%02x", out_md5[i]);
    printf("\n");

    return 0;
}

void MD5_Test(void)
{
    uint8_t res[16];
    // 我们手动模拟 Calculate_File_MD5 的行为
    uint32_t file_length = 6;
    char *test_str       = "123456";

    // 直接调用 Calculate_File_MD5 的核心逻辑
    // 假设我们直接把 RAM 数据喂进去
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);

    // 这里模拟了循环中的 MD5_Update
    MD5_Update(&md5_ctx, (uint8_t *)test_str, file_length);

    MD5_Final(res, &md5_ctx);

    printf("\nExpected: e10adc3949ba59abbe56e057f20f883e\n");
    printf("Test MD5: ");
    for (int i = 0; i < 16; i++) printf("%02x", res[i]);
}
