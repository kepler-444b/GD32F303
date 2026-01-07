#include "app_protocol.h"
#include "../Source/app/app_evenbus/app_eventbus.h"
#include "../Source/bsp/bsp_usart/bsp_usart.h"

// 函数声明
static void app_protocol_check(usart2_rx_buf_t *buf);

void app_protocol_init(void)
{
    bsp_usart_init(USART2, 9600);
    bsp_usart_init(UART4, 9600);
    bsp_usart_init(USART1, 115200);
    bsp_usart2_rx_callback(app_protocol_check); // 注册usart接收回调函数
}

static void app_protocol_check(usart2_rx_buf_t *buf)
{
    app_eventbus_publish(EVENT_USART_RECV_MSG, buf);
}