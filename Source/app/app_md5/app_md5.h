#ifndef _APP_MD5_H_
#define _APP_MD5_H_
#include <stdint.h>

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} MD5_CTX;

void app_md5_init(MD5_CTX *ctx);
void app_md5_update(MD5_CTX *ctx, const uint8_t *input, uint32_t inputLen);
void app_md5_final(uint8_t digest[16], MD5_CTX *ctx);
int app_md5_calculate_file(uint32_t flash_start_addr, uint32_t file_length, char out_md5_str[64]);
#endif