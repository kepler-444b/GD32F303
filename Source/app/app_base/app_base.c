#include "app_base.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

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
