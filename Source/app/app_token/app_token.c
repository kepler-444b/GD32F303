#include "app_token.h"
#include "../Source/app/app_md5/app_md5.h"
#include "../Source/app/app_base/app_base.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"

#include <string.h>
#include <stdio.h>

static void app_token_url_encode(const char *src, char *dst)
{
    const char *hex = "0123456789ABCDEF";
    while (*src) {
        if ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') ||
            (*src >= '0' && *src <= '9') || strchr("-_.~", *src)) {
            *dst++ = *src;
        } else {
            *dst++ = '%';
            *dst++ = hex[((unsigned char)*src) >> 4];
            *dst++ = hex[((unsigned char)*src) & 15];
        }
        src++;
    }
    *dst = '\0';
}

static void app_token_hmac_md5(const uint8_t *key, int key_len, const char *msg, uint8_t out_mac[16])
{
    MD5_CTX ctx;
    uint8_t k_ipad[64] = {0}, k_opad[64] = {0}, tk[16];

    if (key_len > 64) {
        app_md5_init(&ctx);
        app_md5_update(&ctx, key, key_len);
        app_md5_final(tk, &ctx);
        key     = tk;
        key_len = 16;
    }

    memcpy(k_ipad, key, key_len);
    memcpy(k_opad, key, key_len);
    for (int i = 0; i < 64; i++) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }

    // 内层
    app_md5_init(&ctx);
    app_md5_update(&ctx, k_ipad, 64);
    app_md5_update(&ctx, (uint8_t *)msg, strlen(msg));
    app_md5_final(out_mac, &ctx);

    // 外层
    app_md5_init(&ctx);
    app_md5_update(&ctx, k_opad, 64);
    app_md5_update(&ctx, out_mac, 16);
    app_md5_final(out_mac, &ctx);
}

bool app_token_generate(device_info_t *dev)
{
    if (!dev || !dev->key[0] || !dev->products[0] || !dev->version[0] || dev->et == 0) {
        return false;
    }

    char stringForSignature[256];
    uint8_t decoded_key[64];
    uint8_t hmac_res[16];
    char sign_base64[32];
    char res[256];
    char res_enc[256];
    char sign_enc[64];
    int key_len = 0;

    // 严格按照 产品ID/设备名称 拼接 res
    snprintf(res, sizeof(res), "products/%s/devices/%s", dev->products, dev->devices);

    // 拼接签名字符串
    snprintf(stringForSignature, sizeof(stringForSignature), "%u\nmd5\n%s\n%s", dev->et, res, dev->version);

    // Base64 解码 key
    key_len = app_base64_decode(dev->key, decoded_key);
    if (key_len <= 0 || key_len > 64) return false;

    // HMAC-MD5
    app_token_hmac_md5(decoded_key, key_len, stringForSignature, hmac_res);

    // Base64 编码 HMAC
    app_base64_encode(hmac_res, 16, sign_base64);

    // URL 编码
    app_token_url_encode(res, res_enc);
    app_token_url_encode(sign_base64, sign_enc);

    // 拼接最终 token 到 passwd
    snprintf(dev->passwd, sizeof(dev->passwd), "version=%s&res=%s&et=%u&method=md5&sign=%s", dev->version, res_enc, dev->et, sign_enc);

    return true;
}