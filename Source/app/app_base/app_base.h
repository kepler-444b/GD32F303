#ifndef _APP_BASE_H_
#define _APP_BASE_H_

#include <stdbool.h>
#include <stdint.h>

#define UID0        0x1FFFF7E8
#define UID1        0x1FFFF7EC
#define UID2        0x1FFFF7F0

#define BIT0(flag)  ((bool)((flag) & 0x01)) // 第0位
#define BIT1(flag)  ((bool)((flag) & 0x02)) // 第1位
#define BIT2(flag)  ((bool)((flag) & 0x04)) // 第2位
#define BIT3(flag)  ((bool)((flag) & 0x08)) // 第3位
#define BIT4(flag)  ((bool)((flag) & 0x10)) // 第4位
#define BIT5(flag)  ((bool)((flag) & 0x20)) // 第5位
#define BIT6(flag)  ((bool)((flag) & 0x40)) // 第6位
#define BIT7(flag)  ((bool)((flag) & 0x80)) // 第7位

#define L_BIT(byte) ((uint8_t)((byte) & 0x0F))        // 低4位
#define H_BIT(byte) ((uint8_t)(((byte) >> 4) & 0x0F)) // 高4位

uint16_t app_string_to_bytes(const char *str, uint8_t *buf, uint16_t buf_len);
uint16_t app_bytes_to_string(const uint8_t *buf, uint16_t buf_len, char *str);

uint8_t app_panel_frame_crc(uint8_t *rxbuf, uint8_t len);
uint8_t app_panel_frame_sum(uint8_t *rxbuf, uint8_t len);

void app_unpack_bits(const uint8_t *src, int byte_len, char *dst);
void app_pack_bits(const char *src, int bit_len, uint8_t *dst);

void app_base64_encode(const uint8_t *src, int len, char *dst); // base64 编码函数
int app_base64_decode(const char *src, uint8_t *dst);           // base64 解码函数
uint16_t app_crc_16(const uint8_t *buf, uint16_t len);

void app_get_uid(uint8_t uid[12]);

#endif