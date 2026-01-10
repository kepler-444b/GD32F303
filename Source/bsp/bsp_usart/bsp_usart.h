#ifndef _BSP_USART_H_
#define _BSP_USART_H_
#include <stdint.h>
#include <stdio.h>
#include "gd32f30x_usart.h"

// #define RTT_ENABLE // 启用 RTT 调试

#define APP_DEBUG // 此宏用来管理整个工程的 debug 信息

#if defined APP_DEBUG
#define APP_PRINTF(...) printf(__VA_ARGS__)
#define APP_PRINTF_BUF(name, buf, len)                           \
    do {                                                         \
        APP_PRINTF("%s: ", (name));                              \
        for (size_t _idx = 0; _idx < (len); _idx++) {            \
            APP_PRINTF("%02X ", ((const uint8_t *)(buf))[_idx]); \
        }                                                        \
        APP_PRINTF("\n");                                        \
    } while (0)
#define APP_ERROR(fmt, ...) \
    APP_PRINTF("[#%s#] \"" fmt "\" ERROR!\n", __func__, ##__VA_ARGS__)

#else

#define APP_PRINTF(...)
#define APP_PRINTF_BUF(name, buf, len)
#define APP_ERROR(fmt, ...)
#endif

#define USART2_RECV_SIZE 256
#define UART4_RECV_SIZE  128

typedef struct {
    uint8_t buffer[USART2_RECV_SIZE];
    uint16_t length;
} usart2_rx_buf_t;

typedef struct {
    uint8_t buffer[UART4_RECV_SIZE];
    uint16_t length;
} uart4_rx_buf_t;

typedef struct {
    const uint8_t *tx_ptr;
    uint16_t tx_len;
} usart2_tx_buf_t;

typedef void (*usart_rx2_callback_t)(usart2_rx_buf_t *);
typedef void (*uart_rx4_callback_t)(uart4_rx_buf_t *);

void bsp_usart2_rx_callback(usart_rx2_callback_t callback);
void bsp_uart4_rx_callback(uart_rx4_callback_t callback);

void bsp_usart_init(uint32_t usart_com, uint32_t baudrate);
void bsp_usart_tx_buf(const uint8_t *data, uint16_t length, uint32_t usart_com);

#endif