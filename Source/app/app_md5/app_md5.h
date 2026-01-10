#ifndef _APP_MD5_H_
#define _APP_MD5_H_
#include <stdint.h>

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} MD5_CTX;

void MD5_Init(MD5_CTX *ctx);
void MD5_Update(MD5_CTX *ctx, const uint8_t *input, uint32_t inputLen);
void MD5_Final(uint8_t digest[16], MD5_CTX *ctx);
int Calculate_File_MD5(uint32_t flash_start_addr, uint32_t file_length, uint8_t out_md5[16]);
void MD5_Test(void);
#endif