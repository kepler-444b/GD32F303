#include "app_protocol.h"
#include <string.h>
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"

// 函数声明

static void app_usart0_check(usart0_rx_buf_t *buf);
static void app_usart1_check(usart1_rx_buf_t *buf);
static void app_usart2_check(usart2_rx_buf_t *buf);

void app_uart_init_all(void)
{
    bsp_usart_init(USART1, 115200); // 串口屏
    bsp_usart_init(USART0, 115200); // type-c

    bsp_usart_init(USART2, 9600); // 面板通讯
    bsp_usart_init(UART3, 9600);  // 扩展通讯

    bsp_usart0_rx_callback(app_usart0_check); // 注册usart0(type-c)接收回调函数
    bsp_usart1_rx_callback(app_usart1_check); // 注册usart1(串口屏)接收回调函数
    bsp_usart2_rx_callback(app_usart2_check); // 注册usart2(面板通讯)接收回调函数
}

static void app_usart0_check(usart0_rx_buf_t *buf)
{
    if (buf->buffer[0] != USART0_FH_1 || buf->buffer[1] != USART0_FH_2) { // 检查帧头
        return;
    }
    if (buf->buffer[buf->length - 2] != USART0_FT_1 || buf->buffer[buf->length - 1] != USART0_FT_2) { // 检查帧尾
        return;
    }
    uint8_t cmd       = buf->buffer[3];
    uint16_t length   = buf->buffer[2];
    uint8_t *playload = &buf->buffer[4];

    static type_c_rx_t evt_buf; // 这里需要重新使用一个静态区域,以使用evenbus
    memcpy(evt_buf.buffer, playload, length);
    evt_buf.length = length;

    if (cmd == SET_INFO) {
        app_eventbus_publish(EVENT_USART0_SET, &evt_buf);

    } else if (cmd == CFG_INFO) {
        app_eventbus_publish(EVENT_USART0_CFG, &evt_buf); 
    }
}

static void app_usart1_check(usart1_rx_buf_t *buf)
{
    // APP_PRINTF_BUF("recv_from_display", buf->buffer, buf->length);
}

static void app_usart2_check(usart2_rx_buf_t *buf)
{
    app_eventbus_publish(EVENT_USART2_RECV_MSG, buf);
}
