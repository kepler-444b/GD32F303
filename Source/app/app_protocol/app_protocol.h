#ifndef _APP_PROTOCOL_H_
#define _APP_PROTOCOL_H_

void app_uart_init_all(void);
#define USART0_FH_1 0xFF
#define USART0_FH_2 0xAA

#define USART0_FT_1 0x0D
#define USART0_FT_2 0x0A

#define SET_INFO    0x01 // 设置设备信息
#define CFG_INFO    0x02 // 设置场景信息

#endif