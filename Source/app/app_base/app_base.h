#ifndef _APP_BASE_H_
#define _APP_BASE_H_

#include <stdbool.h>
#include <stdint.h>

#define BIT0(flag) ((bool)((flag) & 0x01)) // 第0位
#define BIT1(flag) ((bool)((flag) & 0x02)) // 第1位
#define BIT2(flag) ((bool)((flag) & 0x04)) // 第2位
#define BIT3(flag) ((bool)((flag) & 0x08)) // 第3位
#define BIT4(flag) ((bool)((flag) & 0x10)) // 第4位
#define BIT5(flag) ((bool)((flag) & 0x20)) // 第5位
#define BIT6(flag) ((bool)((flag) & 0x40)) // 第6位
#define BIT7(flag) ((bool)((flag) & 0x80)) // 第7位

uint16_t app_string_to_bytes(const char *str, uint8_t *buf, uint16_t buf_len);
uint8_t app_panel_frame_crc(uint8_t *rxbuf, uint8_t len);
uint8_t app_panel_frame_sum(uint8_t *rxbuf, uint8_t len);

#endif