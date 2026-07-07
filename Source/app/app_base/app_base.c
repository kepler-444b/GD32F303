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

uint16_t app_bytes_to_string(const uint8_t *buf, uint16_t buf_len, char *str)
{
    if (!buf || !str || buf_len == 0) {
        return 0;
    }
    static const char hex[] = "0123456789ABCDEF";
    for (uint16_t i = 0; i < buf_len; i++) {
        str[i * 2]     = hex[buf[i] >> 4];
        str[i * 2 + 1] = hex[buf[i] & 0x0F];
    }
    str[buf_len * 2] = '\0';
    return 1;
}

// 字节转换为二进制字符串
void app_unpack_bits(const uint8_t *src, int byte_len, char *dst)
{
    if (!src || !dst || byte_len <= 0) return;

    int total_bits = byte_len * 8;
    for (int i = 0; i < total_bits; i++) {
        dst[i] = ((src[i >> 3] >> (i & 7)) & 0x01) ? '1' : '0';
    }
    dst[total_bits] = '\0';
}

// 二进制字符串转为字节
void app_pack_bits(const char *src, int bit_len, uint8_t *dst)
{
    if (!src || !dst || bit_len <= 0) return;

    int byte_len = (bit_len + 7) / 8;
    memset(dst, 0, byte_len);

    for (int i = 0; i < bit_len; i++) {
        if (src[i] == '1') {
            dst[i >> 3] |= (1 << (i & 7));
        }
    }
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

uint16_t app_crc_16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001; // 多项式 0x8005 低位先
            else
                crc >>= 1;
        }
    }
    return crc;
}

void app_get_uid(uint8_t uid[12])
{
    uint32_t uid0;
    uint32_t uid1;
    uint32_t uid2;

    uid0 = *(volatile uint32_t *)UID0;
    uid1 = *(volatile uint32_t *)UID1;
    uid2 = *(volatile uint32_t *)UID2;

    uid[0] = uid2 >> 24;
    uid[1] = uid2 >> 16;
    uid[2] = uid2 >> 8;
    uid[3] = uid2;

    uid[4] = uid1 >> 24;
    uid[5] = uid1 >> 16;
    uid[6] = uid1 >> 8;
    uid[7] = uid1;

    uid[8]  = uid0 >> 24;
    uid[9]  = uid0 >> 16;
    uid[10] = uid0 >> 8;
    uid[11] = uid0;
}
