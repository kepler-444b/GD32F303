#include "app_base.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

// Base 编码相关函数
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int get_base64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// Base64 编码
void app_base64_encode(const uint8_t *src, int len, char *dst)
{
    int i, j = 0;
    for (i = 0; i < len; i += 3) {
        int b0 = src[i];
        int b1 = (i + 1 < len) ? src[i + 1] : 0;
        int b2 = (i + 2 < len) ? src[i + 2] : 0;

        dst[j++] = base64_table[b0 >> 2];
        dst[j++] = base64_table[((b0 & 0x03) << 4) | (b1 >> 4)];
        dst[j++] = (i + 1 < len) ? base64_table[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=';
        dst[j++] = (i + 2 < len) ? base64_table[b2 & 0x3F] : '=';
    }
    dst[j] = '\0';
}

// Base64 解码
int app_base64_decode(const char *src, uint8_t *dst)
{
    int len = (int)strlen(src);
    int i, j = 0;
    for (i = 0; i < len; i += 4) {
        int v1 = get_base64_value(src[i]);
        int v2 = get_base64_value(src[i + 1]);
        int v3 = (i + 2 < len && src[i + 2] != '=') ? get_base64_value(src[i + 2]) : 0;
        int v4 = (i + 3 < len && src[i + 3] != '=') ? get_base64_value(src[i + 3]) : 0;

        dst[j++] = (v1 << 2) | (v2 >> 4);
        if (src[i + 2] != '=') dst[j++] = ((v2 & 0x0F) << 4) | (v3 >> 2);
        if (src[i + 3] != '=') dst[j++] = ((v3 & 0x03) << 6) | v4;
    }
    return j;
}

// 把十六进制字符串转换成二进制字节数组
uint16_t app_string_to_bytes(const char *str, uint8_t *buf, uint16_t buf_len)
{
    if (!str || !buf || buf_len == 0) {
        return 0;
    }

    uint16_t i = 0;

    uint16_t str_len = (uint16_t)strlen(str);
    if (str_len == 0 || str_len % 2 != 0) {
        APP_ERROR("str_len error");
        return 0;
    }
    for (uint16_t j = 0; j < str_len; j++) {
        if (!isxdigit((unsigned char)str[j])) {
            APP_ERROR("is not hex");
            return 0;
        }
    }
    for (i = 0; i < buf_len && i < str_len / 2; i++) {
        sscanf(&str[i * 2], "%2hhx", &buf[i]);
    }
    return i;
}

uint8_t app_panel_frame_crc(uint8_t *rxbuf, uint8_t len)
{
    uint8_t i, sum = 0;
    for (i = 0; i < len; i++)
        sum = sum + rxbuf[i];
    return (0xff - sum + 1);
}

uint8_t app_panel_frame_sum(uint8_t *rxbuf, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++)
        sum += rxbuf[i];
    return sum; // 直接返回累加和
}
