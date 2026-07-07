#ifndef _APP_PROTOCOL_H_
#define _APP_PROTOCOL_H_

#include <stdint.h>
void app_uart_init_all(void);
#define USART0_RX_FH_1 0xFF
#define USART0_RX_FH_2 0xAA

#define USART0_TX_FH_1 0xFE
#define USART0_TX_FH_2 0xBB

#define USART0_FT_1    0x0D
#define USART0_FT_2    0x0A

typedef enum {
    GET_INFO = 0x01, // 获取主机信息
    SET_INFO,        // 设置主机信息
    SET_CFG,         // 配置信息
    GET_TIMER,       // 获取定时信息
    SET_TIMER,       // 设置定时信息
    CAL_TIME         // 校准时间
} type_e;

void app_usart0_build(type_e type, uint8_t *data, uint8_t len);
#endif